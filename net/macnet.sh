# Copyright 2007-2008 Roy Marples <roy@marples.name>
# All rights reserved. Released under the 2-clause BSD license.

macnet_depend()
{
	before rename interface wireless
	after macchanger
}

macnet_pre_start()
{
	local mac=$(_get_mac_address 2>/dev/null)
	[ -z "${mac}" ] && return 0

	vebegin "Configuring ${IFACE} for MAC address ${mac}"
	mac=$(echo "${mac}" | sed -e 's/://g')
	_configure_variables "${mac}"
	veend 0
}
