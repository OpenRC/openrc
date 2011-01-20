# Copyright (c) 2007-2008 Roy Marples <roy@marples.name>
# All rights reserved. Released under the 2-clause BSD license.

pump_depend()
{
	program /sbin/pump
	after interface
	provide dhcp
}

_config_vars="$_config_vars dhcp pump"

pump_start()
{
	local args= opt= opts=

	# Get our options
	eval opts=\$dhcp_${IFVAR}
	[ -z "${opts}" ] && opts=${dhcp}

	# Map some generic options to dhcpcd
	for opt in ${opts}; do
		case "${opt}" in
			nodns) args="${args} --no-dns";;
			nontp) args="${args} --no-ntp";;
			nogateway) args="${args} --no-gateway";;
		esac
	done

	# Add our route metric
	[ "${metric:-0}" != "0" ] && args="${args} --route-metric ${metric}"

	args="${args} --win-client-ident"
	args="${args} --keep-up --interface ${IFACE}"

	ebegin "Running pump"
	eval pump "${args}"
	eend $? || return 1

	_show_address
	return 0
}

pump_stop()
{
	# We check for a pump process first as querying for status
	# causes pump to spawn a process
	start-stop-daemon --quiet --test --stop --exec /sbin/pump || return 0

	# Check that pump is running on the interface
	if ! pump --status --interface "${IFACE}" >/dev/null 2>&1; then
		return 0
	fi

	# Pump always releases the lease
	ebegin "Stopping pump on ${IFACE}"
	pump --release --interface "${IFACE}"
	eend $? "Failed to stop pump"
}
