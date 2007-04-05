# Copyright 2004-2007 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2

dhcpcd_depend() {
	after interface
	program start /sbin/dhcpcd
	provide dhcp

	# We prefer dhcpcd over the others
	after dhclient pump udhcpc
}

_config_vars="$_config_vars dhcp dhcpcd"

dhcpcd_start() {
	local args= opt= opts= pidfile="/var/run/dhcpcd-${IFACE}.pid"

	_wait_for_carrier || return 1

	eval args=\$dhcpcd_${IFVAR}

	# Get our options
	eval opts=\$dhcp_${IFVAR}
	[ -z "${opts}" ] && opts=${dhcp}

	# Map some generic options to dhcpcd
	for opt in ${opts} ; do
		case "${opt}" in
			nodns) args="${args} -R" ;;
			nontp) args="${args} -N" ;;
			nonis) args="${args} -Y" ;;
			nogateway) args="${args} -G" ;;
			nosendhost) args="${args} -h ''";
		esac
	done

	# Add our route metric
	[ "${metric:-0}" != "0" ] && args="${args} -m ${metric}"

	# Bring up DHCP for this interface
	ebegin "Running dhcpcd"

	eval /sbin/dhcpcd "${args}" "${IFACE}"
	eend $? || return 1

	_show_address
	return 0
}

dhcpcd_stop() {
	local pidfile="/var/run/dhcpcd-${IFACE}.pid" opts=
	[ ! -f "${pidfile}" ] && return 0

	# Get our options
	if [ -x /sbin/dhcpcd ] ; then
		eval opts=\$dhcp_${IFVAR}
		[ -z "${opts}" ] && opts=${dhcp}
	fi

	ebegin "Stopping dhcpcd on ${IFACE}"
	case " ${opts} " in
		*" release "*) dhcpcd -k "${IFACE}" ;;
		*)
			start-stop-daemon --stop --quiet \
				--exec /sbin/dhcpcd --pidfile "${pidfile}"
			;;
	esac
	eend $?
}

# vim: set ts=4 :
