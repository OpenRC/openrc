# Copyright (c) 2007-2008 Roy Marples <roy@marples.name>
# Released under the 2-clause BSD license.

udhcpc_depend()
{
	program start /bin/busybox
	after interface
	provide dhcp
}

_config_vars="$_config_vars dhcp udhcpc"

udhcpc_start()
{
	local args= opt= opts= pidfile="/var/run/udhcpc-${IFACE}.pid"
	local sendhost=true cachefile="/var/cache/udhcpc-${IFACE}.lease"

	eval args=\$udhcpc_${IFVAR}

	# Get our options
	eval opts=\$dhcp_${IFVAR}
	[ -z "${opts}" ] && opts=${dhcp}

	# This omits the Gentoo specific patch to busybox,
	# but it creates temporary files.
	# We can move this stuff to udhcpc hook script to avoid that, should we do?
	local conf="/var/run/udhcpc-${IFACE}.conf"
	echo -n >"$conf"
	# Map some generic options to dhcpcd
	for opt in ${opts}; do
		case "${opt}" in
			nodns) echo "PEER_DNS=no" >>"$conf" ;;
			nontp) echo "PEER_NTP=no" >>"$conf" ;;
			nogateway) echo "PEER_ROUTERS=no" >>"$conf" ;;
			nosendhost) sendhost=false;
		esac
	done

	[ "${metric:-0}" != "0" ] && echo "IF_METRIC=${metric}" >>"$conf"

	ebegin "Running udhcpc"

	# Try and load the cache if it exists
	if [ -f "${cachefile}" ]; then
		case "$ {args} " in
			*" --request="*|*" -r "*);;
			*)
				local x=$(cat "${cachefile}")
				# Check for a valid ip
				case "${x}" in
					*.*.*.*) args="${args} --request=${x}";;
				esac
				;;
		esac
	fi

	case " ${args} " in
		*" --quit "*|*" -q "*) x="/bin/busybox udhcpc";;
		*) x="start-stop-daemon --start --exec /bin/busybox \
			--pidfile \"${pidfile}\" -- udhcpc";;
	esac

	case " ${args} " in
		*" --hosname="*|*" -h "*|*" -H "*);;
		*)
			if ${sendhost}; then
				local hname="$(hostname)"
				if [ "${hname}" != "(none)" ] && [ "${hname}" != "localhost" ]; then
					args="${args} -x hostname:'${hname}'"
				fi
			fi
			;;
	esac

	eval "${x}" "${args}" --interface="${IFACE}" --now \
		--script="${RC_LIBEXECDIR}/sh/udhcpc-hook.sh" \
		--pidfile="${pidfile}" >/dev/null
	eend $? || return 1

	_show_address
	return 0
}

udhcpc_stop()
{
	local pidfile="/var/run/udhcpc-${IFACE}.pid" opts=
	[ ! -f "${pidfile}" ] && return 0

	# Get our options
	eval opts=\$dhcp_${IFVAR}
	[ -z "${opts}" ] && opts=${dhcp}

	ebegin "Stopping udhcpc on ${IFACE}"
	case " ${opts} " in
		*" release "*)
			start-stop-daemon --stop --quiet --signal USR2 \
				--exec /bin/busybox --pidfile "${pidfile}"
			if [ -f /var/cache/udhcpc-"${IFACE}".lease ]; then
				rm -f /var/cache/udhcpc-"${IFACE}".lease
			fi
			;;
	esac

	start-stop-daemon --stop --exec /bin/busybox --pidfile "${pidfile}"
	eend $?

	if [ -e "/var/run/udhcpc-${IFACE}.conf" ]; then
		rm -f "/var/run/udhcpc-${IFACE}.conf"
	fi
}
