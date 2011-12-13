# Copyright (c) 2007-2008 Roy Marples <roy@marples.name>
# Released under the 2-clause BSD license.

tuntap_depend()
{
	before bridge interface macchanger
}

_config_vars="$_config_vars iproute2 openvpn tunctl"

_is_tuntap()
{
	[ -n "$(export RC_SVCNAME="net.${IFACE}"; service_get_value tuntap)" ]
}

tuntap_pre_start()
{
	local tuntap=
	local rc=
	eval tuntap=\$tuntap_${IFVAR}

	[ -z "${tuntap}" ] && return 0

	if [ ! -e /dev/net/tun ]; then
		if ! modprobe tun; then
			eerror "TUN/TAP support is not present in this kernel"
			return 1
		fi
		vebegin "Waiting for /dev/net/tun"
		# /dev/net/tun can take its time to appear
		local timeout=10
		while [ ! -e /dev/net/tun -a ${timeout} -gt 0 ]; do
			sleep 1
			: $(( timeout -= 1 ))
		done
		if [ ! -e /dev/net/tun ]; then
			eerror "TUN/TAP support present but /dev/net/tun is not"
			return 1
		fi
		veend 0
	fi

	ebegin "Creating Tun/Tap interface ${IFACE}"

	# Set the base metric to 1000
	metric=1000

	local i_opts= o_opts= t_opts=
	local do_iproute2=false do_openvpn=false do_tunctl=false
	eval i_opts=\$iproute2_${IFVAR}
	eval o_opts=\$openvpn_${IFVAR}
	eval t_opts=\$tunctl_${IFVAR}

	if [ -n "${i_opts}" ] && type ip >/dev/null 2>&1; then
		do_iproute2=true
	elif [ -n "${o_opts}" ] && type openvpn >/dev/null 2>&1; then
		do_openvpn=true
	elif [ -n "${t_opts}" ] && type tunctl >/dev/null 2>&1; then
		do_tunctl=true
	elif type ip >/dev/null 2>&1; then
		do_iproute2=true
	elif type openvpn >/dev/null 2>&1; then
		do_openvpn=true
	elif type tunctl >/dev/null 2>&1; then
		do_tunctl=true
	fi

	if ${do_iproute2}; then
		ip tuntap add dev "${IFACE}" mode "${tuntap}" ${i_opts}
		rc=$?
	elif ${do_openvpn}; then
		openvpn --mktun --dev-type "${tuntap}" --dev "${IFACE}" \
			${o_opts} >/dev/null
		rc=$?
	elif ${do_tunctl}; then
		tunctl ${t_opts} -t "${IFACE}" >/dev/null
		rc=$?
	else
		eerror "Neither iproute2, openvpn nor tunctl has been found, please install"
		eerror "either \"iproute2\" \"openvpn\" or \"usermode-utilities\"."
		rc=1
	fi
	eend $rc && _up && service_set_value tuntap "${tuntap}"
}

tuntap_post_stop()
{
	_is_tuntap || return 0

	ebegin "Destroying Tun/Tap interface ${IFACE}"
	if type ip > /dev/null 2>&1; then
		ip tuntap del dev ${IFACE} mode $(service_get_value tuntap)
	elif type tunctl >/dev/null 2>&1; then
		tunctl -d "${IFACE}" >/dev/null
	else
		openvpn --rmtun \
			--dev-type "$(service_get_value tuntap)" \
			--dev "${IFACE}" >/dev/null
	fi
	eend $?
}
