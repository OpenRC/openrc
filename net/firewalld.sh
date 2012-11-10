# Copyright (c) 2012 Doug Goldstein <cardoe@cardoe.com>
# Released under the 2-clause BSD license.

firewalld_depend()
{
	after interface
	before dhcp
	program firewall-cmd
	[ "$IFACE" != "lo" ] && need firewalld
}

_config_vars="$_config_vars firewalld_zone"

firewalld_post_start()
{
	local firewalld_zone=
	eval firewalld_zone=\$firewalld_zone_${IFVAR}

	_exists || return 0

	if [ "${IFACE}" != "lo" ]; then
		firewall-cmd --zone="${firewalld_zone}" \
			--change-interface="${IFACE}" > /dev/null 2>&1
	fi

	return 0
}

firewalld_pre_stop()
{
	_exists || return 0

	if [ "${IFACE}" != "lo" ]; then
		firewall-cmd --remove-interface="${IFACE}" > /dev/null 2>&1
	fi

	return 0
}
