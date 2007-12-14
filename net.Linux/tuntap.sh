# Copyright 2007 Roy Marples
# All rights reserved

# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.

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

	if [ ! -e /dev/net/tun ]; then
		modprobe tun && sleep 1
		if [ ! -e /dev/net/tun ]; then
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
	eend $? && _up && save_options tuntap "${tuntap}"
}

tuntap_post_stop() {
	_is_tuntap || return 0

	ebegin "Destroying Tun/Tap interface ${IFACE}"
	if type tunctl >/dev/null 2>&1; then
		tunctl -d "${IFACE}" >/dev/null
	else
		openvpn --rmtun \
			--dev-type "$(get_options tuntap)" \
			--dev "${IFACE}" >/dev/null
	fi
	eend $?
}

# vim: set ts=4 :
