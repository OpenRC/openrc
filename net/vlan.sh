# Copyright (c) 2007-2008 Roy Marples <roy@marples.name>
# Released under the 2-clause BSD license.

vlan_depend()
{
	local x
	x=$(_which ip)
	[ -z "$x" ] && return 1
	program $x
	after interface
	before dhcp
}

_config_vars="$_config_vars vlans"

_is_vlan()
{
	[ ! -d /proc/net/vlan ] && return 1
	[ -e /proc/net/vlan/"${IFACE}" ] && return 0
	grep -Eq "^${IFACE}[[:space:]]+" /proc/net/vlan/config
}

_get_vlans()
{
	[ -e /proc/net/vlan/config ] || return 1
	sed -n -e 's/^\W*\([^ ]*\) \(.* \) .*'"${IFACE}"'$/\1/p' /proc/net/vlan/config
}

_check_vlan()
{
	if [ ! -d /proc/net/vlan ]; then
		modprobe 8021q
		if [ ! -d /proc/net/vlan ]; then
			eerror "VLAN (802.1q) support is not present in this kernel"
			return 1
		fi
	fi
}

vlan_pre_start()
{
	local vconfig
	eval vconfig=\$vconfig_${IFVAR}
	if [ -n "${vconfig}" ]; then
		eerror "You must convert your vconfig_ VLAN entries to vlan${N} entries."
		return 1
	fi
	local vlans=
	eval vlans=\$vlans_${IFVAR}
	[ -z "$vlans" ] && return 0
	case " ${MODULES} " in
		*" ifconfig "*)
				eerror "sys-apps/iproute2 is required to configure VLANs"
				return 1 ;;
	esac
}

vlan_post_start()
{
	local vlans=
	eval vlans=\$vlans_${IFVAR}
	[ -z "${vlans}" ] && return 0

	_check_vlan || return 1
	_exists || return 1

	local vlan= e= s= vname= vflags= vingress= vegress=
	for vlan in ${vlans}; do
		einfo "Adding VLAN ${vlan} to ${IFACE}"
		# We need to gather all interface configuration options
		# 1) naming. Default to the standard "${IFACE}.${vlan}" but it can be anything
		eval vname=\$vlan${vlan}_name
		[ -z "${vname}" ] && vname="${IFACE}.${vlan}"
		# 2) flags
		eval vflags=\$vlan${vlan}_flags
		# 3) ingress/egress map
		eval vingress=\$vlan${vlan}_ingress
		[ -z "${vingress}" ] || vingress="ingress-qos-map ${vingress}"
		eval vegress=\$vlan${vlan}_egress
		[ -z "${vegress}" ] || vegress="egress-qos-map ${vegress}"

		local txqueuelen=
		eval txqueuelen=\$txqueuelen_vlan${vlan}
		local mac=
		eval mac=\$mac_vlan${vlan}
		local broadcast=
		eval broadcast=\$broadcast_vlan${vlan}
		local mtu=
		eval mtu=\$mtu_vlan${vlan}
		local opts="${txqueuelen:+txqueuelen} ${txqueuelen} ${mac:+address} ${mac} ${broadcast:+broadcast} ${broadcast} ${mtu:+mtu} ${mtu}"

		e="$(ip link add link "${IFACE}" name "${vname}" ${opts} type vlan id "${vlan}" ${vflags} ${vingress} ${vegress} 2>&1 1>/dev/null)"
		if [ -n "${e}" ]; then
			eend 1 "${e}"
			continue
		fi

		# We may not want to start the vlan ourselves
		eval s=\$vlan_start_${IFVAR}
		yesno ${s:-yes} || continue

		# We need to work out the interface name of our new vlan id
		local ifname="$(sed -n -e \
			's/^\([^[:space:]]*\) *| '"${vlan}"' *| .*'"${IFACE}"'$/\1/p' \
			/proc/net/vlan/config )"
		mark_service_started "net.${ifname}"
		(
			export RC_SVCNAME="net.${ifname}"
			start
		) || mark_service_stopped "net.${ifname}"
	done

	return 0
}

vlan_pre_stop()
{
	local vlan=

	_exists || return 0

	for vlan in $(_get_vlans); do
		einfo "Removing VLAN ${vlan##*.} from ${IFACE}"
		(
			export RC_SVCNAME="net.${vlan}"
			stop
		) && {
			mark_service_stopped "net.${vlan}"
			ip link delete "${vlan}" type vlan >/dev/null
		}
	done

	return 0
}
