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
	local proto= address= foo=
	LC_ALL=C ifconfig "${IFACE}" | while read proto address foo; do
		case "${proto}" in
			ether) 
				case "${address}" in
					00:00:00:00:00:00);;
					44:44:44:44:44:44);;
					FF:FF:FF:FF:FF:FF);;
					*) echo "${address}";;
				esac
				return 0
				;;
		esac
	done
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
			for x in $(ifconfig -l); do
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

_ifconfig_ent() {
	LC_ALL=C ifconfig "${IFACE}" 2>/dev/null | while read ent rest; do
   		case "${ent}" in
			"$1") echo "${rest}";;
		esac
	done
}

_is_wireless() {
	case "$(_ifconfig_ent "media:")" in
		"IEEE 802.11 Wireless"*) return 0;;
		*) return 1;;
	esac
}

_get_inet_address() {
	local inet= address= n= netmask= rest=
	LC_ALL=C ifconfig "${IFACE}" | while read inet address n netmask rest; do
		if [ "${inet}" = "inet" ]; then
			echo "${address}/$(_netmask2cidr "${netmask}")"
			return 0
		fi
	done
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
		OpenBSD)
			local ip="${1%%/*}" cidr="${1##*/}" netmask=
			if [ -n "${cidr}" -a "${cidr}" != "${ip}" ]; then
				netmask="$(_cidr2netmask "${cidr}")"
				shift
				set -- "${ip}" netmask "${netmask}" "$@"
			fi
			;;
	esac

	ifconfig "${IFACE}" ${inet6} "$@" alias
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
	einfo "Removing addresses"
	eindent
	LC_ALL=C ifconfig "${IFACE}" | while read inet address rest; do
		case "${inet}" in
			inet|inet6)
				case "${address}" in
					*"%${IFACE}"|::1) continue;;
					127.0.0.1) [ "${IFACE}" = "lo0" ] && continue;;
				esac
				einfo "${address}"
				ifconfig "${IFACE}" "${inet}" "${address}" -alias
				eend $?
				;;
		esac
	done
	eoutdent
	return 0
}

_show_address() {
	einfo "received address $(_get_inet_address "${IFACE}")"
}

_has_carrier() {
	case "$(_ifconfig_ent "status:")" in
		""|active|associated) return 0;;
		*) return 1;;
	esac
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

_ifconfig_ipv6_tentative() {
	local inet= address= rest=
	LC_ALL=C ifconfig "${IFACE}" | while read inet address rest; do
	 	case "${inet}" in
			inet6)
				case "${rest}" in
					*" "tentative*) return 2;;
				esac
				;;
		esac
	done
	[ $? = 2 ]
}

ifconfig_post_start() {
	if _ifconfig_ipv6_tentative; then
		ebegin "Waiting for IPv6 addresses"
		while true; do
			_ifconfig_ipv6_tentative || break
		done
		eend 0
	fi
}

# vim: set ts=4 :
