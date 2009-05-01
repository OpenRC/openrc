# Copyright (c) 2007-2008 Roy Marples <roy@marples.name>
# All rights reserved. Released under the 2-clause BSD license.

ssidnet_depend()
{
	before interface system
	after wireless
}

ssidnet_pre_start()
{
	[ -z "${SSID}" -a -z "${SSIDVAR}" ] && return 0

	local mac=$(_get_ap_mac_address | sed -e 's/://g') x=

	vebegin "Configuring ${IFACE} for SSID ${SSID}"
	_configure_variables "${mac}" "${SSIDVAR}"

	# Backwards compat for old gateway var
	eval x=\$gateway_${SSIDVAR}
	[ -n "${x}" ] && gateway=${x}

	veend 0
}
