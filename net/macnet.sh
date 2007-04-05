# Copyright 2005-2007 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2

macnet_depend() {
	before rename interface wireless
	after macchanger
}

macnet_pre_start() {
	local mac=$(_get_mac_address 2>/dev/null)
	[ -z "${mac}" ] && return 0

	vebegin "Configuring ${IFACE} for MAC address ${mac}"
	mac=$(echo "${mac}" | sed -e 's/://g')
	_configure_variables "${mac}"
	veend 0
}

# vim: set ts=4 :
