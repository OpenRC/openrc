# Copyright 2004-2007 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2

ifconfig_depend() {
	program /sbin/ifconfig
	provide interface
}

_up() {
	ifconfig "${IFACE}" up
}

_down() {
	ifconfig "${IFACE}" down
}

_exists() {
	grep -Eq "^[[:space:]]*${IFACE}:" /proc/net/dev
}

_ifindex() {
	local line= i=-2
	while read line ; do
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

_get_mac_address() {
	local mac=$(LC_ALL=C ifconfig "${IFACE}" | \
	sed -n -e 's/.* HWaddr \(..:..:..:..:..:..\).*/\1/p')


	case "${mac}" in
		00:00:00:00:00:00) ;;
		44:44:44:44:44:44) ;;
		FF:FF:FF:FF:FF:FF) ;;
		"") ;;
		*) echo "${mac}"; return 0 ;;
	esac

	return 1
}

_set_mac_address() {
	ifconfig "${IFACE}" hw ether "$1"
}

_get_inet_address() {
	set -- $(LC_ALL=C ifconfig "${IFACE}" |
	sed -n -e 's/.*inet addr:\([^ ]*\).*Mask:\([^ ]*\).*/\1 \2/p')
	[ -z "$1" ] && return 1

	echo -n "$1"
	shift
	echo "/$(_netmask2cidr "$1")"
}

_get_inet_addresses() {
	local iface=${IFACE} i=0
	local addrs="$(_get_inet_address)"

	while true ; do
		local IFACE="${iface}:${i}"
		_exists || break
		local addr="$(_get_inet_address)"
		[ -n "${addr}" ] && addrs="${addrs}${addrs:+ }${addr}"
		i=$((${i} + 1))
	done
	echo "${addrs}"
}

_cidr2netmask() {
	local cidr="$1" netmask="" done=0 i=0 sum=0 cur=128
	local octets= frac=

	local octets=$((${cidr} / 8))
	local frac=$((${cidr} % 8))
	while [ ${octets} -gt 0 ] ; do
		netmask="${netmask}.255"
		octets=$((${octets} - 1))
		done=$((${done} + 1))
	done

	if [ ${done} -lt 4 ] ; then
		while [ ${i} -lt ${frac} ] ; do
			sum=$((${sum} + ${cur}))
			cur=$((${cur} / 2))
			i=$((i + 1))
		done
		netmask="${netmask}.${sum}"
		done=$((${done} + 1))

		while [ ${done} -lt 4 ] ; do
			netmask="${netmask}.0"
			done=$((${done} + 1))
		done
	fi

	echo "${netmask#.*}"
}

_add_address() {
	if [ "$1" = "127.0.0.1/8" -a "${IFACE}" = "lo" ] ; then
		ifconfig "${IFACE}" "$@" 2>/dev/null
		return 0
	fi

	case "$1" in 
		*:*) ifconfig "${IFACE}" inet6 add "$@"; return $?;;
	esac

	# IPv4 is tricky - ifconfig requires an aliased device
	# for multiple addresses
	local iface="${IFACE}"
	if LC_ALL=C ifconfig "${iface}" | grep -Eq "\<inet addr:.*" ; then
		# Get the last alias made for the interface and add 1 to it
		i=$(ifconfig | sed '1!G;h;$!d' | grep -m 1 -o "^${iface}:[0-9]*" \
			| sed -n -e 's/'"${iface}"'://p')
		i=$((${i:-0} + 1))
		iface="${iface}:${i}"
	fi

	# ifconfig doesn't like CIDR addresses
	local ip="${1%%/*}" cidr="${1##*/}" netmask=
	if [ -n "${cidr}" -a "${cidr}" != "${ip}" ]; then
		netmask="$(_cidr2netmask "${cidr}")"
		shift
		set -- "${ip}" netmask "${netmask}" "$@"
	fi

	local arg= cmd=
	while [ -n "$1" ] ; do
		case "$1" in
			brd)
				if [ "$2" = "+" ] ; then
					shift
				else
					cmd="${cmd} broadcast"
				fi
				;;
			peer) cmd="${cmd} pointtopoint";;
			*) cmd="${cmd} $1" ;;
		esac
		shift
	done

	ifconfig "${iface}" ${cmd}
}

_add_route() {
	local inet6=

	if [ -n "${metric}" ] ; then
		set -- "$@" metric ${metric}
	fi

	if [ $# -eq 3 ] ; then
		set -- "$1" "$2" gw "$3"
	elif [ "$3" = "via" ] ; then
		local one=$1 two=$2
		shift ; shift; shift
		set -- "${one}" "${two}" gw "$@"
	fi

	case "$@" in
		*:*)
			inet6="-A inet6"
			[ "$1" = "-net" ] && shift
			;;
	esac

	route ${inet6} add "$@" dev "${IFACE}"
}

_delete_addresses() {
	# We don't remove addresses from aliases
	case "${IFACE}" in
		*:*) return 0 ;;
	esac

	einfo "Removing addresses"
	eindent
	# iproute2 can add many addresses to an iface unlike ifconfig ...
	# iproute2 added addresses cause problems for ifconfig
	# as we delete an address, a new one appears, so we have to
	# keep polling
	while true ; do
		local addr=$(_get_inet_address)
		[ -z "${addr}" ] && break
		
		if [ "${addr}" = "127.0.0.1/8" ] ; then
			# Don't delete the loopback address
			[ "${IFACE}" = "lo" -o "${IFACE}" = "lo0" ] && break
		fi
		einfo "${addr}"
		ifconfig "${IFACE}" 0.0.0.0 || break
	done

	# Remove IPv6 addresses
	local addr=
	for addr in $(LC_ALL=C ifconfig "${IFACE}" | \
		sed -n -e 's/^.*inet6 addr: \([^ ]*\) Scope:[^L].*/\1/p') ; do
		[ "${addr}" = "::1/128" -a "${IFACE}" = "lo" ] && continue
		einfo "${addr}"
		ifconfig "${IFACE}" inet6 del "${addr}"
	done
	
	return 0
}

_has_carrier() {
	return 0
}

_tunnel() {
	iptunnel "$@"
}

ifconfig_pre_start() {
	# MTU support
	local mtu=
	eval mtu=\$mtu_${IFVAR}
	[ -n "${mtu}" ] && ifconfig "${IFACE}" mtu "${mtu}"

	local tunnel=

	eval tunnel=\$iptunnel_${IFVAR}
	[ -z "${tunnel}" ] && return 0

	# Set our base metric to 1000
	metric=1000
	
	ebegin "Creating tunnel ${IFVAR}"
	iptunnel add "${tunnel}"
	eend $?
}

ifconfig_post_stop() {
	# Don't delete sit0 as it's a special tunnel
	[ "${IFACE}" = "sit0" ] && return 0

	[ -z "$(iptunnel show "${IFACE}" 2>/dev/null)" ] && return 0

	ebegin "Destroying tunnel ${IFACE}"
	iptunnel del "${IFACE}"
	eend $?
}

# vim: set ts=4 :
