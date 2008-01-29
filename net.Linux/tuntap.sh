# Copyright 2007-2008 Roy Marples <roy@marples.name>
# All rights reserved. Released under the 2-clause BSD license.

tuntap_depend()
{
	before bridge interface macchanger
}

_config_vars="$_config_vars tunctl"

_is_tuntap()
{
	[ -n "$(export SVCNAME="net.${IFACE}"; service_get_value tuntap)" ]
}

tuntap_pre_start()
{
	local tuntap=
	eval tuntap=\$tuntap_${IFVAR}

	[ -z "${tuntap}" ] && return 0

	if [ ! -e /dev/net/tun ]; then
		if ! modprobe tun; then
			eerror "TUN/TAP support is not present in this kernel"
			return 1
		fi
	fi

	ebegin "Creating Tun/Tap interface ${IFACE}"

	# Set the base metric to 1000
	metric=1000

	if type tunctl >/dev/null 2>&1; then
		local opts=
		eval opts=\$tunctl_${IFVAR}
		tunctl ${opts} -t "${IFACE}" >/dev/null
	else
		openvpn --mktun --dev-type "${tuntap}" --dev "${IFACE}" >/dev/null
	fi
	eend $? && _up && service_set_value tuntap "${tuntap}"
}

tuntap_post_stop()
{
	_is_tuntap || return 0

	ebegin "Destroying Tun/Tap interface ${IFACE}"
	if type tunctl >/dev/null 2>&1; then
		tunctl -d "${IFACE}" >/dev/null
	else
		openvpn --rmtun \
			--dev-type "$(service_get_value tuntap)" \
			--dev "${IFACE}" >/dev/null
	fi
	eend $?
}
