# Copyright 2004-2007 Gentoo Foundation
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

iproute2_depend() {
	program /sbin/ip
	provide interface
	after ifconfig
}

_up() {
	ip link set up dev "${IFACE}"
}

_down() {
	ip link set down dev "${IFACE}"
}

_exists() {
	grep -Eq "^[[:space:]]*${IFACE}:" /proc/net/dev
}

_ifindex() {
	local line= i=-2
	while read line; do
		i=$((${i} + 1))
		[ ${i} -lt 1 ] && continue
		case "${line}" in
			"${IFACE}:"*) echo "${i}"; return 0;;
		esac
	done < /proc/net/dev

	# Return the next available index
	i=$((${i} + 1))
	echo "${i}"
	return 1
}

_is_wireless() {
	# Support new sysfs layout
	[ -d /sys/class/net/"${IFACE}"/wireless ] && return 0

	[ ! -e /proc/net/wireless ] && return 1
	grep -Eq "^[[:space:]]*${IFACE}:" /proc/net/wireless
}

_set_flag() {
	local flag=$1 opt="on"
	if [ "${flag#-}" != "${flag}" ]; then
		flag=${flag#-}
		opt="off"
	fi
	ip link set "${IFACE}" "${flag}" "${opt}"
}

_get_mac_address() {
	local mac=$(LC_ALL=C ip link show "${IFACE}" | sed -n \
		-e 'y/abcdef/ABCDEF/' \
		-e '/link\// s/^.*\<\(..:..:..:..:..:..\)\>.*/\1/p')

	case "${mac}" in
		00:00:00:00:00:00);;
		44:44:44:44:44:44);;
		FF:FF:FF:FF:FF:FF);;
		"");;
		*) echo "${mac}"; return 0;;
	esac

	return 1
}

_set_mac_address() {
	ip link set address "$1" dev "${IFACE}"
}

_get_inet_addresses() {
	LC_ALL=C ip -family inet addr show "${IFACE}" | \
	sed -n -e 's/.*inet \([^ ]*\).*/\1/p'
}

_get_inet_address() {
	set -- $(_get_inet_addresses)
	[ $# = "0" ] && return 1
	echo "$1"
}

_add_address() {
	if [ "$1" = "127.0.0.1/8" -a "${IFACE}" = "lo" ]; then
		ip addr add "$@" dev "${IFACE}" 2>/dev/null
		return 0
	fi

	# Convert an ifconfig line to iproute2
	if [ "$2" = "netmask" ]; then
		local one="$1" three="$3"
		shift; shift; shift
		set -- "${one}/$(_netmask2cidr "${three}")" "$@"
	fi
	
	#config=( "${config[@]//pointopoint/peer}" )
	
	# Always scope lo addresses as host unless specified otherwise
	if [ "${IFACE}" = "lo" ]; then
		set -- "$@" "scope" "host"
	fi

	# IPv4 specifics
	case "$1" in
		*.*.*.*)
			case "$@" in
				*" brd "*);;
				*" broadcast "*);;
				*) set -- "$@" brd +;;
			esac
			;;
	esac

	ip addr add dev "${IFACE}" "$@"
}

_add_route() {
	if [ $# -eq 3 ]; then
		set -- "$1" "$2" via "$3"
	elif [ "$3" = "gw" ]; then
		local one=$1 two=$2
		shift; shift; shift
		set -- "${one}" "${two}" gw "$@"
	fi

	local cmd= have_metric=false 
	while [ -n "$1" ]; do
		case "$1" in
			metric) cmd="${cmd} $1"; have_metric=true;;
			netmask) cmd="${cmd}/$(_netmask2cidr "$2")"; shift;;
			-host|-net);;
			-A)	[ "$2" = "inet6" ] && shift;;
			*) cmd="${cmd} $1";;
		esac
		shift
	done

	if ! ${have_metric} && [ -n "${metric}" ]; then
		cmd="${cmd} metric ${metric}"
	fi

	ip route append ${cmd} dev "${IFACE}"
	eend $?
}

_delete_addresses() {
	ip addr flush dev "${IFACE}" scope global 2>/dev/null
	ip addr flush dev "${IFACE}" scope site 2>/dev/null
	if [ "${IFACE}" != "lo" ]; then
		ip addr flush dev "${IFACE}" scope host 2>/dev/null
	fi
	return 0
}

_has_carrier() {
	return 0
}

_tunnel() {
	ip tunnel "$@"
}

iproute2_pre_start() {
	# MTU support
	local mtu=
	eval mtu=\$mtu_${IFVAR}
	[ -n "${mtu}" ] && ip link set mtu "${mtu}" dev "${IFACE}"

	# TX Queue Length support
	local len=
	eval len=\$txqueuelen_${IFVAR}
	[ -n "${len}" ] && ip link set txqueuelen "${len}" dev "${IFACE}"

	local tunnel=
	eval tunnel=\$iptunnel_${IFVAR}
	if [ -n "${tunnel}" ]; then
		# Set our base metric to 1000
		metric=1000

		ebegin "Creating tunnel ${IFVAR}"
		ip tunnel add ${tunnel} name "${IFACE}"
		eend $? || return 1
		_up	
	fi

	return 0
}

_iproute2_ipv6_tentative() {
		LC_ALL=C ip addr show dev "${IFACE}" | \
		grep -q "^[[:space:]]*inet6 .* tentative"
}

iproute2_post_start() {
	# Kernel may not have IP built in
	if [ -e /proc/net/route ]; then
		ip route flush table cache dev "${IFACE}"
	fi

	if _iproute2_ipv6_tentative; then
		ebegin "Waiting for IPv6 addresses"
		while true; do
			_iproute2_ipv6_tentative || break
		done
		eend 0
	fi
}

iproute2_post_stop() {
	# Don't delete sit0 as it's a special tunnel
	if [ "${IFACE}" != "sit0" ]; then
		if [ -n "$(ip tunnel show "${IFACE}" 2>/dev/null)" ]; then
			ebegin "Destroying tunnel ${IFACE}"
			ip tunnel del "${IFACE}"
			eend $?
		fi
	fi
}

# vim: set ts=4 :
