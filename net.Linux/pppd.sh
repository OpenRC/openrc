# Copyright 2005-2007 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2

pppd_depend() {
	program /usr/sbin/pppd
	after interface
	before dhcp
	provide ppp
}

is_ppp() {
	[ -e /var/run/ppp-"${IFACE}".pid ]
}

requote() {
	printf "'%s' " "$@"
}

pppd_start() {
	${IN_BACKGROUND} && return 0

	if [ "${IFACE%%[0-9]*}" != "ppp" ] ; then
		eerror "PPP can only be invoked from net.ppp[0-9]"
		return 1
	fi

	local link= i= opts= unit="${IFACE#ppp}" mtu=
	if [ -z "${unit}" ] ; then
		eerror "PPP requires a unit - use net.ppp[0-9] instead of net.ppp"
		return 1
	fi

	# PPP requires a link to communicate over - normally a serial port
	# PPPoE communicates over Ethernet
	# PPPoA communicates over ATM
	# In all cases, the link needs to be available before we start PPP
	eval link=\$link_${IFVAR}
	if [ -z "${link}" ] ; then
		eerror "link_${IFVAR} has not been set in /etc/conf.d/net"
		return 1
	fi

	case "${link}" in
		/*)
			if [ ! -e "${link}" ] ; then
				eerror "${link} does not exist"
				eerror "Please verify hardware or kernel module (driver)"
				return 1
			fi
			;;
	esac

	eval $(_get_array "pppd_${IFVAR}")
	opts="$@"

	local mtu= hasmtu=false hasmru=false hasmaxfail=false haspersits=false
	local hasupdetach=false
	for i in "$@" ; do
		set -- ${i}
		case "$1" in
			unit|nodetach|linkname)
				eerror "The option \"$1\" is not allowed in pppd_${IFVAR}"
				return 1
			;;
			mtu) hasmtu=true ;;
			mru) hasmru=true ;;
			maxfail) hasmaxfail=true ;;
			persist) haspersist=true ;;
			updetach) hasupdetach=true;
		esac
	done

	# Might be set in conf.d/net
	local username= password= passwordset=
	eval username=\$username_${IFVAR}
	eval password=\$password_${IFVAR}
	eval passwordset=\${password_${IFVAR}-x}
	if [ -n "${username}" ] \
	&& [ -n "${password}" -o -z "${passwordset}" ] ; then
		opts="${opts} plugin passwordfd.so passwordfd 0"
	fi
	
	if [ -n "${mtu}" ] ; then
		${hasmtu} || opts="${opts} mtu ${mtu}"
		${hasmru} || opts="${opts} mru ${mtu}"
	fi
	${hasmailfail} || opts="${opts} maxfail 0"
	${haspersist} || opts="${opts} persist"

	# Set linkname because we need /var/run/ppp-${linkname}.pid
	# This pidfile has the advantage of being there,
	# even if ${IFACE} interface was never started
	opts="linkname ${IFACE} ${opts}"

	# Setup auth info
	if [ -n "${username}" ] ; then
		opts="user '${username}' remotename ${IFACE} ${opts}"
	fi

	# Load a custom interface configuration file if it exists
	[ -f "/etc/ppp/options.${IFACE}" ] \
		&& opts="${opts} file /etc/ppp/options.${IFACE}"

	# Set unit
	opts="unit ${unit} ${opts}"
	
	# Setup connect script
	local chatopts="/usr/sbin/chat -e -E -v"
	eval $(_get_array "phone_number_${IFVAR}")
	[ -n "$1" ] && chatopts="${chatopts} -T '$1'"
	[ -n "$2" ] && chatopts="${chatopts} -U '$2'"
	eval $(_get_array "chat_${IFVAR}")
	if [ -n "$@" ] ; then
		opts="${opts} connect $(printf "'%s' " "${chatopts} $(printf "'%s' " "$@")")"
	fi

	# Add plugins
	local haspppoa=false haspppoe=false
	eval $(_get_array "plugins_${IFVAR}")
	for i in "$@" ; do
		set -- ${i}
		case "$1" in
			passwordfd) continue;;
			pppoa) shift; set -- "pppoatm" "$@" ;;
			pppoe) shift; set -- "rp-pppoe" "$@" ;;
			capi) shift; set -- "capiplugin" "$@" ;;
		esac
		case "$1" in
			rp-pppoe) haspppoe=true ;;
			pppoatm)  haspppoa=true ;;
		esac
		if [ "$1" = "rp-pppoe" ] || [ "$1" = "pppoatm" -a "${link}" != "/dev/null" ] ; then
			opts="${opts} connect true"
			set -- "$@" "${link}"
		fi
		opts="${opts} plugin $1.so"
		shift
		opts="${opts} $@"
	done

	#Specialized stuff. Insert here actions particular to connection type (pppoe,pppoa,capi)
	local insert_link_in_opts=1
	if ${haspppoe} ; then
		if [ ! -e /proc/net/pppoe ] ; then
			# Load the PPPoE kernel module
			if ! modprobe pppoe ; then
				eerror "kernel does not support PPPoE"
				return 1
			fi
		fi

		# Ensure that the link exists and is up
		( IFACE="${link}" ; _exists true && _up ) || return 1
		insert_link_in_opts=0
	fi

	if ${haspppoa} ; then
		if [ ! -d /proc/net/atm ] ; then
			# Load the PPPoA kernel module
			if ! modprobe pppoatm ; then
				eerror "kernel does not support PPPoATM"
				return 1
			fi
		fi

		if [ "${link}" != "/dev/null" ] ; then
			insert_link_in_opts=0
		else
			ewarn "WARNING: An [itf.]vpi.vci ATM address was expected in link_${IFVAR}"
		fi

	fi
	[ "${insert_link_in_opts}" = "0" ] || opts="${link} ${opts}"

	ebegin "Starting pppd in ${IFACE}"
	mark_service_inactive "${SVCNAME}"
	if [ -n "${username}" ] \
	&& [ -n "${password}" -o -z "${passwordset}" ] ; then
		printf "${password}" | sed -e 's/\\/\\\\/g' -e 's/"/\\"/g' | \
		eval start-stop-daemon --start --exec /usr/sbin/pppd \
			--pidfile "/var/run/ppp-${IFACE}.pid" -- "${opts}" >/dev/null
	else
		eval start-stop-daemon --start --exec /usr/sbin/pppd \
			--pidfile "/var/run/ppp-${IFACE}.pid" -- "${opts}" >/dev/null
	fi

	if ! eend $? "Failed to start PPP" ; then
		mark_service_starting "net.${IFACE}"
		return 1
	fi

	if ${hasupdetach} ; then
		_show_address
	else
		einfo "Backgrounding ..."
	fi

	# pppd will re-call us when we bring the interface up
	exit 0
}

pppd_stop() {
	${IN_BACKGROUND} && return 0
	local pidfile="/var/run/ppp-${IFACE}.pid"

	[ ! -s "${pidfile}" ] && return 0

	# Give pppd at least 30 seconds do die, #147490
	einfo "Stopping pppd on ${IFACE}"
	start-stop-daemon --stop --quiet --exec /usr/sbin/pppd \
		--pidfile "${pidfile}" --retry 30
	eend $?
}

# vim: set ts=4 :
