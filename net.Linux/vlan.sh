# Copyright 2004-2007 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2

vlan_depend() {
	program /sbin/vconfig
	after interface
	before dhcp
}

_config_vars="$_config_vars vlans"

_is_vlan() {
	[ ! -d /proc/net/vlan ] && return 1
	[ -e /proc/net/vlan/"${IFACE}" ] && return 0
	grep -Eq "^${IFACE}[[:space:]]+" /proc/net/vlan/config
}

_get_vlans() {
	[ -e /proc/net/vlan/config ] || return 1
	sed -n -e 's/^\(.*[0-9]\) \(.* \) .*'"${IFACE}"'$/\1/p' /proc/net/vlan/config
}

_check_vlan() {
	if [ ! -d /proc/net/vlan ] ; then
		modprobe 8021q
		if [ ! -d /proc/net/vlan ] ; then
			eerror "VLAN (802.1q) support is not present in this kernel"
			return 1
		fi
	fi
}

vlan_pre_start() {
	local vc="$(_get_array "vconfig_${IFVAR}")"
	[ -z "${vc}" ] && return 0

	_check_vlan || return 1
	_exists || return 1

	local v= x= e=
	local IFS="$__IFS"
	for v in ${vc}; do
		unset IFS
		case "${v}" in
			set_name_type" "*) x=${v} ;;
			*) x="$(echo "${v}" | sed -e "s/ / ${IFACE} /g")"
			   [ "${x}" = "${v}" ] && x="${x} ${IFACE}"
			   ;;
		esac

		set -x
		e="$(vconfig ${x} 2>&1 1>/dev/null)"
		set +x
		[ -z "${e}" ] && continue
		eerror "${e}"
		return 1
	done
}

vlan_post_start() {
	local vlans=
	eval vlans=\$vlans_${IFACE}
	[ -z "${vlans}" ] && return 0
	
	_check_vlan || return 1
	_exists || return 1

	local vlan= e= s=
	for vlan in ${vlans}; do
		einfo "Adding VLAN ${vlan} to ${IFACE}"
		e="$(vconfig add "${IFACE}" "${vlan}" 2>&1 1>/dev/null)"
		if [ -n "${e}" ] ; then
			eend 1 "${e}"
			continue
		fi

		# We may not want to start the vlan ourselves
		eval s=\$vlan_start_${IFVAR}
		[ "${s:-yes}" != "yes" ] && continue

		# We need to work out the interface name of our new vlan id
		local ifname="$( \
			sed -n -e 's/^\([^[:space:]]*\) *| '"${vlan}"' *| .*'"${iface}"'$/\1/p' \
			/proc/net/vlan/config )"
		mark_service_started "net.${ifname}"
		(
			export SVCNAME="net.${ifname}"
			start	
		) || mark_service_stopped "net.${ifname}"
	done
	
	return 0
}

vlan_post_stop() {
	local vlan=

	for vlan in $(_get_vlans); do
		einfo "Removing VLAN ${vlan##*.} from ${IFACE}"
		(
			export SVCNAME="net.${vlan}"
			stop
		) && {
			mark_service_stopped "net.${vlan}"
			vconfig rem "${vlan}" >/dev/null
		}
	done

	return 0
}

# vim: set ts=4 :
