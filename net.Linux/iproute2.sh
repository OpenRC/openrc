# Copyright 2004-2007 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2

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
	while read line ; do
		i=$((${i} + 1))
		[ ${i} -lt 1 ] && continue
		case "${line}" in
			"${IFACE}:"*) echo "${i}"; return 0;;
		esac
	done < /proc/net/dev
	return 1
}

_is_wireless() {
	# Support new sysfs layout
	[ -d /sys/class/net/"${IFACE}"/wireless ] && return 0

	[ ! -e /proc/net/wireless ] && return 1
	grep -Eq "^[[:space:]]*${IFACE}:" /proc/net/wireless
}

_get_mac_address() {
	local mac=$(LC_ALL=C ip link show "${IFACE}" | sed -n \
		-e 'y/abcdef/ABCDEF/' \
		-e '/link\// s/^.*\<\(..:..:..:..:..:..\)\>.*/\1/p')

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
	if [ "$1" = "127.0.0.1/8" -a "${IFACE}" = "lo" ] ; then
		ip addr add "$@" dev "${IFACE}" 2>/dev/null
		return 0
	fi

	# Convert an ifconfig line to iproute2
	if [ "$2" = "netmask" ] ; then
		local one="$1" three="$3"
		shift ; shift ; shift
		set -- "${one}/$(_netmask2cidr "${three}")" "$@"
	fi
	
	#config=( "${config[@]//pointopoint/peer}" )
	
	# Always scope lo addresses as host unless specified otherwise
	if [ "${IFACE}" = "lo" ] ; then
		set -- "$@" "scope" "host"
	fi

	# IPv4 specifics
	case "$1" in
		*.*.*.*)
			case "$@" in
				*" brd "*) ;;
				*" broadcast "*) ;;
				*) set -- "$@" brd + ;;
			esac
			;;
	esac

	ip addr add dev "${IFACE}" "$@"
}

_add_route() {
	if [ $# -eq 3 ] ; then
		set -- "$1" "$2" via "$3"
	elif [ "$3" = "gw" ] ; then
		local one=$1 two=$2
		shift ; shift; shift
		set -- "${one}" "${two}" gw "$@"
	fi

	local cmd= have_metric=false 
	while [ -n "$1" ] ; do
		case "$1" in
			metric) cmd="${cmd} $1"; have_metric=true ;;
			netmask) cmd="${cmd}/$(_netmask2cidr "$2")"; shift ;;
			-host|-net) ;;
			-A)	[ "$2" = "inet6" ] && shift ;;
			*) cmd="${cmd} $1" ;;
		esac
		shift
	done

	if ! ${have_metric} && [ -n "${metric}" ] ; then
		cmd="${cmd} metric ${metric}"
	fi

	ip route append ${cmd} dev "${IFACE}"
	eend $?
}

_delete_addresses() {
	ip addr flush dev "${IFACE}" scope global 2>/dev/null
	ip addr flush dev "${IFACE}" scope site 2>/dev/null
	if [ "${IFACE}" != "lo" ] ; then
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

	local tunnel=
	eval tunnel=\$iptunnel_${IFVAR}
	if [ -n "${tunnel}" ] ; then
		# Set our base metric to 1000
		metric=1000

		ebegin "Creating tunnel ${IFVAR}"
		ip tunnel add ${tunnel} name "${IFACE}"
		eend $? || return 1
		_up	
	fi

	return 0
}

iproute2_post_start() {
	ip route flush cache dev "${IFACE}"
}

iproute2_post_stop() {
	# Don't delete sit0 as it's a special tunnel
	if [ "${IFACE}" != "sit0" ] ; then
		if [ -n "$(ip tunnel show "${IFACE}" 2>/dev/null)" ] ; then
			ebegin "Destroying tunnel ${IFACE}"
			ip tunnel del "${IFACE}"
			eend $?
		fi
	fi
}

# vim: set ts=4 :
