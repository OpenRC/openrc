# Copyright 2004-2007 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2

tuntap_depend() {
	before bridge interface macchanger
}

_config_vars="$_config_vars tunctl"

_is_tuntap() {
	[ -n "$(get_options tuntap "${SVCNAME}")" ]
}

tuntap_pre_start() {
	local tuntap=
	eval tuntap=\$tuntap_${IFVAR}

	[ -z "${tuntap}" ] && return 0

	if [ ! -e /dev/net/tun ] ; then
		modprobe tun && sleep 1
		if [ ! -e /dev/net/tun ] ; then
			eerror "TUN/TAP support is not present in this kernel"
			return 1
		fi
	fi

	ebegin "Creating Tun/Tap interface ${IFACE}"

	# Set the base metric to 1000
	metric=1000
	
	if [ -x /usr/sbin/openvpn ] ; then
		openvpn --mktun --dev-type "${tuntap}" --dev "${IFACE}" > /dev/null
	else
		local opts=
		eval opts=\$tunctl_${IFVAR}
		tunctl ${opts} -t "${IFACE}" >/dev/null
	fi
	eend $? && _up && save_options tuntap "${tuntap}"
}

tuntap_post_stop() {
	_is_tuntap || return 0

	ebegin "Destroying Tun/Tap interface ${IFACE}"
	if [ -x /usr/sbin/openvpn ] ; then
		openvpn --rmtun \
			--dev-type "$(get_options tuntap)" \
			--dev "${IFACE}" > /dev/null
	else
		tunctl -d "${IFACE}" >/dev/null
	fi
	eend $?
}

# vim: set ts=4 :
