# Copyright 2004-2007 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2

udhcpc_depend() {
	program start /sbin/udhcpc
	after interface
	provide dhcp
}

_config_vars="$_config_vars dhcp udhcpc"

udhcpc_start() {
	local args= opt= opts= pidfile="/var/run/udhcpc-${IFACE}.pid"
	local sendhost=true cachefile="/var/cache/udhcpc-${IFACE}.lease"

	eval args=\$udhcpc_${IFVAR}

	# Get our options
	eval opts=\$dhcp_${IFVAR}
	[ -z "${opts}" ] && opts=${dhcp}

	# Map some generic options to dhcpcd
	for opt in ${opts} ; do
		case "${opt}" in
			nodns) args="${args} --env PEER_DNS=no" ;;
			nontp) args="${args} --env PEER_NTP=no" ;;
			nogateway) args="${args} --env PEER_ROUTERS=no" ;;
			nosendhost) sendhost=false;
		esac
	done

	[ "${metric:-0}" != "0" ] && args="${args} --env IF_METRIC=${metric}"

	ebegin "Running udhcpc"

	# Try and load the cache if it exists
	if [ -f "${cachefile}" ]; then
		case "$ {args} " in
			*" --request="*|*" -r "*) ;;
			*)
				local x=$(cat "${cachefile}")
				# Check for a valid ip
				case "${x}" in
					*.*.*.*) args="${args} --request=${x}" ;;
				esac
				;;
		esac
	fi

	case " ${args} " in
		*" --quit "*|*" -q "*) x="/sbin/udhcpc" ;;
		*) x="start-stop-daemon --start --exec /sbin/udhcpc \
			--pidfile \"${pidfile}\" --" ;;
	esac

	case " ${args} " in
		*" --hosname="*|*" -h "*|*" -H "*) ;;
		*)
			if ${sendhost} ; then
				local hname="$(hostname)"
				if [ "${hname}" != "(none)" ] && [ "${hname}" != "localhost" ]; then
					args="${args} --hostname='${hname}'"
				fi
			fi
			;;
	esac

	eval "${x}" "${args}" --interface="${IFACE}" --now \
		--script="${RC_LIBDIR}"/sh/udhcpc.sh \
		--pidfile="${pidfile}" >/dev/null
	eend $? || return 1

	_show_address
	return 0
}

udhcpc_stop() {
	local pidfile="/var/run/udhcpc-${IFACE}.pid" opts= sig="TERM"
	[ ! -f "${pidfile}" ] && return 0

	# Get our options
	eval opts=\$dhcp_${IFVAR}
	[ -z "${opts}" ] && opts=${dhcp}

	ebegin "Stopping udhcpc on ${IFACE}"
	case " ${opts} " in
		*" release "*)
			sig="USR2"
			if [ -f /var/cache/udhcpc-"${IFACE}".lease ] ; then
				rm -f /var/cache/udhcpc-"${IFACE}".lease
			fi
			;;
	esac

	start-stop-daemon --stop --quiet --signal "${sig}" \
		--exec /sbin/udhcpc --pidfile "${pidfile}"
	eend $?
}

# vim: set ts=4 :
