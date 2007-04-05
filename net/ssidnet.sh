# Copyright 2004-2007 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2

ssidnet_depend() {
	before interface system
	after wireless
}

ssidnet_pre_start() {
	[ -z "${SSID}" -a -z "${SSIDVAR}" ] && return 0

	local mac=$(_get_ap_mac_address | sed -e 's/://g') x=

	vebegin "Configuring ${IFACE} for SSID ${SSID}"
	_configure_variables "${mac}" "${SSIDVAR}"

	# Backwards compat for old gateway var
	eval x=\$gateway_${SSIDVAR}
	[ -n "${x}" ] && gateway=${x}

	veend 0
}

# vim: set ts=4 :
