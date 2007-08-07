# Copyright 2004-2007 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2

ifconfig_depend() {
	program /sbin/ifconfig
	provide interface
}

_exists() {
	# Only FreeBSD sees to have /dev/net .... is there something
	# other than ifconfig we can use for the others?
	if [ -d /dev/net ] ; then
		[ -e /dev/net/"${IFACE}" ]
	else
		ifconfig "${IFACE}" >/dev/null 2>&1
	fi
}

_get_mac_address() {
	local mac=$(LC_ALL=C ifconfig "${IFACE}" | \
	sed -n -e 's/^[[:space:]]*ether \(..:..:..:..:..:..\).*/\1/p')

	case "${mac}" in
		00:00:00:00:00:00) ;;
		44:44:44:44:44:44) ;;
		FF:FF:FF:FF:FF:FF) ;;
		*) echo "${mac}"; return 0 ;;
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
			for x in /dev/net[0-9]* ; do
				if [ "${x}" -ef /dev/net/"${IFACE}" ] ; then
					echo "${x#/dev/net}"
					return 0
				fi
				i=$((${i} + 1))
			done
			;;
		default)
			for x in $(ifconfig -a | sed -n -e 's/^\([^[:space:]]*\):.*/\1/p') ; do
				if [ "${x}" = "${IFACE}" ] ; then
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
		*:*) inet6=inet6 ;;
	esac

	if [ "${metric:-0}" != "0" ] ; then
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
	if [ $# -gt 3 ] ; then
		if [ "$3" = "gw" -o "$3" = "via" ] ; then
			local one=$1 two=$2
			shift ; shift; shift
			set -- "${one}" "${two}" "$@"
		fi
	fi

	case "$@" in
		*:*) route add -inet6 "$@" ;;
		*)   route add        "$@" ;;
	esac
}

_delete_addresses() {
	# We don't remove addresses from aliases
	case "${IFACE}" in
		*:*) return 0 ;;
	esac

	einfo "Removing addresses"
	eindent
	local addr=
	for addr in $(LC_ALL=C ifconfig "${IFACE}" |
		sed -n -e 's/^[[:space:]]*inet \([^ ]*\).*/\1/p') ; do
		if [ "${addr}" = "127.0.0.1" ] ; then
			# Don't delete the loopback address
			[ "$1" = "lo" -o "$1" = "lo0" ] && continue
		fi
		einfo "${addr}"
		ifconfig "$1" delete "${addr}"
		eend $?
	done

	# Remove IPv6 addresses
	for addr in $(LC_ALL=C ifconfig "${IFACE}" | \
		sed -n -e 's/^[[:space:]]*inet6 \([^ ]*\).*/\1/p') ; do
		case "${addr}" in
			*"%${IFACE}") continue ;;
			::1) continue ;;
		esac
		einfo "${addr}"
		ifconfig "${IFACE}" inet6 delete "${addr}"
		eend $?
	done
	
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

# vim: set ts=4 :
