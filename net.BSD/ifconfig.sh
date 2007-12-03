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

ifconfig_depend() {
	program /sbin/ifconfig
	provide interface
}

_exists() {
	# Only FreeBSD sees to have /dev/net .... is there something
	# other than ifconfig we can use for the others?
	if [ -d /dev/net ]; then
		[ -e /dev/net/"${IFACE}" ]
	else
		ifconfig "${IFACE}" >/dev/null 2>&1
	fi
}

_get_mac_address() {
	local mac=$(LC_ALL=C ifconfig "${IFACE}" | \
	sed -n -e 's/^[[:space:]]*ether \(..:..:..:..:..:..\).*/\1/p')

	case "${mac}" in
		00:00:00:00:00:00);;
		44:44:44:44:44:44);;
		FF:FF:FF:FF:FF:FF);;
		*) echo "${mac}"; return 0;;
	esac

	return 1
}

_up () {
	ifconfig "${IFACE}" up
}

_down () {
	ifconfig "${IFACE}" down
}

_ifindex() {
	local x= i=1
	case "${RC_UNAME}" in
		FreeBSD|DragonFly)
			for x in /dev/net[0-9]*; do
				if [ "${x}" -ef /dev/net/"${IFACE}" ]; then
					echo "${x#/dev/net}"
					return 0
				fi
				i=$((${i} + 1))
			done
			;;
		default)
			for x in $(ifconfig -a | sed -n -e 's/^\([^[:space:]]*\):.*/\1/p'); do
				if [ "${x}" = "${IFACE}" ]; then
					echo "${i}"
					return 0
				fi
				i=$((${i} + 1))
			done
			;;
	esac

	# Return the next available index
	echo "${i}"
	return 1
}

_is_wireless() {
	LC_ALL=C ifconfig "${IFACE}" 2>/dev/null | \
		grep -q "^[[:space:]]*media: IEEE 802.11 Wireless"
}

_get_inet_address() {
	set -- $(LC_ALL=C ifconfig "${IFACE}" |
	sed -n -e 's/^[[:space:]]*inet \([^ ]*\) netmask 0x\(..\)\(..\)\(..\)\(..\).*/\1 0x\2.0x\3.0x\4/p')
	[ -z "$1" ] && return 1

	echo -n "$1"
	shift
	echo "/$(_netmask2cidr "$1")"
}

_add_address() {
	local inet6=

	case "$@" in
		*:*) inet6=inet6;;
	esac

	if [ "${metric:-0}" != "0" ]; then
		set -- "$@" metric ${metric}
	fi

	# ifconfig doesn't like CIDR addresses
	case "${RC_UNAME}" in
		NetBSD|OpenBSD)
			local ip="${1%%/*}" cidr="${1##*/}" netmask=
			if [ -n "${cidr}" -a "${cidr}" != "${ip}" ]; then
				netmask="$(_cidr2netmask "${cidr}")"
				shift
				set -- "${ip}" netmask "${netmask}" "$@"
			fi
			;;
	esac

	ifconfig "${IFACE}" ${inet6} alias "$@"
}

_add_route() {
	if [ $# -gt 3 ]; then
		if [ "$3" = "gw" -o "$3" = "via" ]; then
			local one=$1 two=$2
			shift; shift; shift
			set -- "${one}" "${two}" "$@"
		fi
	fi

	case "$@" in
		*:*) route add -inet6 "$@";;
		*)   route add        "$@";;
	esac
}

_delete_addresses() {
	# We don't remove addresses from aliases
	case "${IFACE}" in
		*:*) return 0;;
	esac

	einfo "Removing addresses"
	eindent
	local addr=
	for addr in $(LC_ALL=C ifconfig "${IFACE}" |
		sed -n -e 's/^[[:space:]]*inet \([^ ]*\).*/\1/p'); do
		if [ "${addr}" = "127.0.0.1" ]; then
			# Don't delete the loopback address
			[ "$1" = "lo" -o "$1" = "lo0" ] && continue
		fi
		einfo "${addr}"
		ifconfig "$1" delete "${addr}"
		eend $?
	done

	# Remove IPv6 addresses
	for addr in $(LC_ALL=C ifconfig "${IFACE}" | \
		sed -n -e 's/^[[:space:]]*inet6 \([^ ]*\).*/\1/p'); do
		case "${addr}" in
			*"%${IFACE}") continue;;
			::1) continue;;
		esac
		einfo "${addr}"
		ifconfig "${IFACE}" inet6 delete "${addr}"
		eend $?
	done
	eoutdent
	
	return 0
}

_show_address() {
	einfo "received address $(_get_inet_address "${IFACE}")"
}

_has_carrier() {
	local s=$(LC_ALL=C ifconfig "${IFACE}" | \
	sed -n -e 's/^[[:space:]]status: \(.*\)$/\1/p')
	[ -z "${s}" -o "${s}" = "active" -o "${s}" = "associated" ] 
}

ifconfig_pre_start() {
	local config="$(_get_array "ifconfig_${IFVAR}")" conf= arg= args=
	local IFS="$__IFS"

	[ -z "${config}" ] && return 0

	veinfo "Running ifconfig commands"
	eindent
	for conf in ${config}; do
		unset IFS
		args=
		for arg in ${conf}; do
			case ${arg} in
				[Dd][Hh][Cc][Pp]);;
				[Nn][Oo][Aa][Uu][Tt][Oo]);;
				[Nn][Oo][Ss][Yy][Nn][Cc][Dd][Hh][Cc][Pp]);;
				[Ss][Yy][Nn][Cc][Dd][Hh][Cc][Pp]);;
				[Ww][Pp][Aa]);;
				*) args="${args} ${arg}";;
			esac
		done

		[ -z "${args}" ] && continue
		vebegin "ifconfig${args}"
		eval ifconfig "${IFACE}" "${args}"
		veend $?
	done
	eoutdent

	return 0
}

ifconfig_post_start() {
	vebegin "Waiting for IPv6 addresses"
	while true; do
		LC_ALL=C ifconfig "${IFACE}" | grep -q "^[[:space:]]*inet6 .* tentative" || break
	done
	veend 0
}

# vim: set ts=4 :
