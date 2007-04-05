# Copyright 2004-2007 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2

ifconfig_depend() {
	program /sbin/ifconfig
	provide interface
}

_exists() {
	[ -e /dev/net/"${IFACE}" ]
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
	local x=
	for x in /dev/net[0-9]* ; do
		if [ "${x}" -ef /dev/net/"${IFACE}" ] ; then
			echo "${x#/dev/net}"
			return 0
		fi
	done
	return 1
}

_is_wireless() {
	LC_ALL=C ifconfig "${IFACE}" 2>/dev/null | \
		grep -q "^[[:space:]]*media: IEEE 802.11 Wireless"
}

_get_inet_address() {
	set -- $(LC_ALL=C ifconfig "${IFACE}" |
	sed -n -e 's/^[[:space:]]*inet \([^ ]*\) netmask 0x\(..\)\(..\)\(..\)\(..\).*/\1 0x\2.0x\3.0x\4/p')
	echo -n "$1"
	shift

	echo "/$(_netmask2cidr "$1")"
}

_add_address() {
	if [ "${metric:-0}" != "0" ] ; then
		set -- "$@" metric ${metric}
	fi

	ifconfig "${IFACE}" add "$@"
}

_add_route() {
	if [ $# -gt 3 ] ; then
		if [ "$3" = "gw" -o "$3" = "via" ] ; then
			local one=$1 two=$2
			shift ; shift; shift
			set -- "${one}" "${two}" "$@"
		fi
	fi

	route add "$@"
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
		/sbin/ifconfig "$1" delete "${addr}"
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
		/sbin/ifconfig "${IFACE}" inet6 delete "${addr}"
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
