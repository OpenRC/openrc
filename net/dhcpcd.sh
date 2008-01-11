# Copyright 2007-2008 Roy Marples <roy@marples.name>
# All rights reserved. Released under the 2-clause BSD license.

dhcpcd_depend()
{
	after interface
	program start dhcpcd
	provide dhcp

	# We prefer dhcpcd over the others
	after dhclient pump udhcpc
}

_config_vars="$_config_vars dhcp dhcpcd"

dhcpcd_start()
{
	local args= opt= opts= pidfile="/var/run/dhcpcd-${IFACE}.pid"

	eval args=\$dhcpcd_${IFVAR}

	# Get our options
	eval opts=\$dhcp_${IFVAR}
	[ -z "${opts}" ] && opts=${dhcp}

	# Map some generic options to dhcpcd
	for opt in ${opts}; do
		case "${opt}" in
			nodns) args="${args} -R";;
			nontp) args="${args} -N";;
			nonis) args="${args} -Y";;
			nogateway) args="${args} -G";;
			nosendhost) args="${args} -h ''";
		esac
	done

	# Add our route metric
	[ "${metric:-0}" != "0" ] && args="${args} -m ${metric}"

	# Bring up DHCP for this interface
	ebegin "Running dhcpcd"

	eval dhcpcd "${args}" "${IFACE}"
	eend $? || return 1

	_show_address
	return 0
}

dhcpcd_stop()
{
	local pidfile="/var/run/dhcpcd-${IFACE}.pid" opts= sig=SIGTERM
	[ ! -f "${pidfile}" ] && return 0

	ebegin "Stopping dhcpcd on ${IFACE}"
	eval opts=\$dhcp_${IFVAR}
	[ -z "${opts}" ] && opts=${dhcp}
	case " ${opts} " in
		*" release "*) sig=SIGHUP;;
	esac
	start-stop-daemon --stop --quiet --signal ${sig} --pidfile "${pidfile}"
	eend $?
}
