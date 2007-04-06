# Copyright 2004-2007 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2

_config_vars="$_config_vars essid mode associate_timeout preferred_aps blacklist_aps"

iwconfig_depend() {
    program /sbin/ifconfig
    after plug
    before interface
    provide wireless
}

iwconfig_get_wep_status() {
    local status="disabled"
    local mode=$(LC_ALL=C ifconfig "${IFACE}" \
    | sed -n -e 's/^[[:space:]]*authmode \([^ ]*\) privacy ON .*/\1/p')
    if [ -n "${mode}" ] ; then
	status="enabled - ${mode}"
    fi

    echo "(WEP ${status})"
}

_iwconfig_get() {
    LC_ALL=C ifconfig "${IFACE}" | \
    sed -n -e 's/^[[:space:]]*ssid \(.*\) channel \([0-9]*\) bssid \(..:..:..:..:..:..\)$/\'"$1"'/p'
}

_get_ssid() {
    _iwconfig_get 1
}

_get_ap_mac_address() {
    _iwconfig_get 3
}

_get_channel() {
    _iwconfig_get 2
}

iwconfig_report() {
    local m="connected to"
    local ssid=$(_get_ssid)
    local mac=$(_get_ap_mac_address "${iface}")
    [ -n "${mac}" ] && mac=" at ${mac}"
    local wep_status="$(iwconfig_get_wep_status "${iface}")"
    local channel=$(_get_channel)
    [ -n "${channel}" ] && channel="on channel ${channel} "

    eindent
    einfo "${IFACE} ${m} \"${ssid}\"${mac}"
    einfo "${channel}${wep_status}"
    eoutdent
}

iwconfig_get_wep_key() {
    local mac="$1" key=
    [ -n "${mac}" ] && mac="$(echo "${mac}" | sed -e 's/://g')"
    eval key=\$mac_key_${mac}
    [ -z "${key}" ] && eval key=\$key_${SSIDVAR}
    echo "${key:--}"
}

iwconfig_user_config() {
    local conf=
    eval set -- \$ifconfig_${SSIDVAR}
    for conf in "$@" ; do
	ifconfig "${IFACE}" ${conf}
    done
}

iwconfig_set_mode() {
    local x= opt= unopt="hostap adhoc"
    case "${mode}" in
	master|hostap) unopt="adhoc" opt="hostap" ;;
	ad-hoc|adhoc) unopt="hostap" opt="adhoc" ;;
    esac
    for x in ${unopt} ; do
	ifconfig "${IFACE}" -mediaopt ${x}
    done
    for x in ${opt} ; do
	ifconfig "${IFACE}" mediaopt ${x}
    done
}

iwconfig_setup_specific() {
    local mode="${1:-master}" channel=
    if [ -z "${SSID}" ]; then
	eerror "${IFACE} requires an SSID to be set to operate in ${mode} mode"
	eerror "adjust the ssid_${IFVAR} setting in /etc/conf.d/net"
	return 1
    fi

    iwconfig_set_mode "${mode}" || return 1

    SSIDVAR=$(_shell_var "${SSID}")
    local key=$(iwconfig_get_wep_key)

    # Now set the key
    ifconfig "${IFACE}" wepkey ${key}

    ifconfig "${IFACE}" ssid "${ESSID}" || return 1

    eval channel=\$channel_${IFVAR}
    # We default the channel to 3
    ifconfig "${IFACE}" channel "${channel:-3}" || return 1

    iwconfig_user_config
    iwconfig_report "${iface}"
    return 0
}

iwconfig_associate() {
    local mac="$1" channel="$2" caps="$3"
    local mode= w="(WEP Disabled)" key=

    SSIDVAR=$(_shell_var "${SSID}")
    key=$(iwconfig_get_wep_key "${mac}")
    case "${caps}" in
	[EI]P*)
	    if [ "${key}" = "-" ] ; then
		ewarn "WEP key is not set for \"${SSID}\"; not connecting"
		return 1
	    fi
	    ;;
	"") ;;
	*)
	    if [ "${key}" != "-" ] ; then
		key="-"
		ewarn "\"${ESSID}\" is not WEP enabled; ignoring setting"
	    fi
	    ;;
    esac

    # Set mode accordingly
    case "${caps}" in
	*E*) mode="managed"; ifconfig "${IFACE}" -mediaopt adhoc ;;
	*I*) mode="adhoc"; ifconfig "${IFACE}" mediaopt adhoc ;;
	*)
	    if LC_ALL=C ifconfig "${IFACE}" | grep -q "^[[:space:]]*media: .*adhoc" ; then
		mode="adhoc"
	    else
		mode="managed"
	    fi
	    ;;
    esac

    if [ "${key}" = "-" ] ; then
	ifconfig "${IFACE}" wepmode off
    else
	ifconfig "${IFACE}" wepmode on
	ifconfig "${IFACE}" deftxkey 1
	w=$(iwconfig_get_wep_status)
    fi
    
    ebegin "Connecting to \"${SSID}\" in ${mode} mode ${w}"
   
    if ! ifconfig "${IFACE}" wepkey ${key} ; then
	eerror "Invalid WEP key ${key}"
	return 1
    fi

    ifconfig "${IFACE}" ssid "${SSID}" || return 1
    iwconfig_user_config

    if [ "${SSID}" != "any" ] && type preassociate >/dev/null 2>/dev/null ; then
	veinfo "Running preassociate function"
	veindent
	( preassociate )
	local e=$?
	veoutdent
	if [ ${e} -eq 0 ] ; then
	    veend 1 "preassociate \"${SSID}\" on ${IFACE} failed"
	    return 1
	fi
    fi

    local timeout= i=0
    eval timeout=\$associate_timeout_${IFVAR}
    timeout=${timeout:-10}

    [ ${timeout} -eq 0 ] \
    && vewarn "WARNING: infinite timeout set for association on ${IFACE}"

    while true; do
	_has_carrier && break
	sleep 1
	[ ${timeout} -eq 0 ] && continue
	i=$((${i} +  1))
	[ ${i} -ge ${timeout} ] && return 1
    done

    if ! _has_carrier ; then
	eend 1
	return 1
    fi
    eend 0

    if [ "${SSID}"  = "any" ]; then
	SSID="$(_get_ssid)"
	iwconfig_associate
	return $?
    fi

    iwconfig_report

    if type postassociate >/dev/null 2>/dev/null ; then
	veinfo "Running postassociate function"
	veindent
	( postassociate )
	veoutdent
    fi

    return 0
}

iwconfig_scan() {
	local x= i=0 scan= quality=
	einfo "Scanning for access points"
	eindent

	scan="$(LC_ALL=C ifconfig -v "${IFACE}" list scan 2>/dev/null | sed -e "1 d" -e "s/$/'/g" -e "s/^/'/g")"
	while [ ${i} -lt 3 -o -z "${scan}" ] ; do
	    scan="${scan}${scan:+ }$(LC_ALL=C ifconfig -v "${IFACE}" scan 2>/dev/null | sed -e "1 d" -e "s/$/'/g" -e "s/^/'/g")"
	    i=$((${i} + 1))
	done
	
	local OIFS=$IFS
	APS=-1
	eval set -- ${scan}
	for line in "$@" ; do
	    APS=$((${APS} + 1))
	    set -- ${line}
	    while true ; do
		case "$1" in
		    *:*:*:*:*:*) break ;;
		esac
		eval SSID_${APS}="\"\${SSID_${APS}}\${SSID_${APS}:+ }$1\""
		shift
	    done
	    eval MAC_${APS}="$(echo "$1" | tr '[:lower:]' '[:upper:]')"
	    eval CHAN_${APS}=$2
	    quality=${4%:*}
	    shift ; shift ; shift ; shift ; shift
	    eval CAPS_${APS}=$*

	    # Add 1000 for managed nodes as we prefer them to adhoc
	    set -- $*
	    case "$1" in
		*E*) eval QUAL_${APS}=$((${quality} + 1000)) ;;
		*)   eval QUAL_${APS}=\$quality ;;
	    esac
	done

	if [ -z "${MAC_0}" ]; then
		ewarn "no access points found"
		eoutdent
		return 1
	fi

	# Sort based on quality
	local i=0 k=1 a= b= x= t=
	while [ ${i} -lt ${APS} ] ; do
	    k=$((${i} + 1))
	    while [ ${k} -le ${APS} ] ; do
		eval a=\$QUALITY_${i}
		[ -z "${a}" ] && break
		eval b=\$QUALITY_${k}
		if [ -n "${b}" -a "${a}" -lt "${b}" ] ; then
		    for x in MAC SSID CHAN QUALITY CAPS ; do
			eval t=\$${x}_${i}
			eval ${x}_${i}=\$${x}_${k}
			eval ${x}_${k}=\$t
		    done
		fi
		k=$((${k} + 1))
	    done
	    i=$((${i} + 1))
	done

	# Strip any duplicates
	local i=0 k=1 a= b=
	while [ ${i} -lt ${APS} ] ; do
	    k=$((${i} + 1))
	    while [ ${k} -le ${APS} ] ; do
		eval a=\$MAC_${i}
		eval b=\$MAC_${k}
		if [ "${a}" = "${b}" ] ; then
		    eval a=\$QUALITY_${i}
		    eval b=\$QUALITY_${k}
		    if [ -n "${a}" -a -n "${b}" ] ; then
			if [ ${a} -ge ${b} ] ; then
			    unset MAC_${k} SSID_${k} CHAN_${k} QUALITY_${k} CAPS_${k}
			else
			    unset MAC_${i} SSID_${i} CHAN_${i} QUALITY_${i} CAPS_${i}
			fi
		    else
			unset MAC_${k} SSID_${k} CHAN_${k} QUALITY_${k} CAPS_${k}
		    fi
		fi
		k=$((${k} + 1))
	    done
	    i=$((${i} + 1))
	done

	local i=0 e= m= black= s=
	eval "$(_get_array "blacklist_aps")"
	black="$@"

	while [ ${i} -le ${APS} ] ; do
		eval x=\$MAC_${i}
		if [ -z "${x}" ] ; then
		    i=$((${i} + 1))
		    continue
		fi

		eval m=\$MODE_${i}
		[ -n "${m}" ] && m=", ${m}"
		eval s=\$SSID_${i}
		eval q=\$QUALITY_${i}
		eval e=\$CAPS_${i}
		case "${e}" in
		    [EI]P*) e=", encrypted" ;;
		    *) e="" ;;
		esac
		if [ -z "${s}" ] ; then
			einfo "Found ${x}${m}${e}"
		else
			einfo "Found \"${s}\" at ${x}${m}${e}"
		fi

		x="$(echo "${x}" | sed -e 's/://g')"
		eval x=\$mac_ssid_${x}
		if [ -n "${x}" ] ; then
			eval SSID_${i}=\$x
			s=${x}
			eindent
			einfo "mapping to \"${x}\""
			eoutdent
		fi

		eval "$(_get_array "blacklist_aps")"
		for x in "$@" ; do
			if [ "${x}" = "${s}" ] ; then
				ewarn "${s} has been blacklisted - not connecting"
				unset SSID_${i} MAC_${i} CHAN_${i} QUALITY_${i} CAPS_${i}
			fi
		done
		i=$((${i} + 1))
	done
	eoutdent
	return 0
}

iwconfig_force_preferred() {
	[ -z "${preferred_aps}" ] && return 1

	ewarn "Trying to force preferred in case they are hidden"
	eval "(_get_array "preferred_aps")"
	local ssid=
	for ssid in "$@"; do
		local found_AP=false i=0 e=
		while [ ${i} -le ${APS} ] ; do
			eval e=\$SSID_${i}
			if [ "${e}" = "${ssid}" ] ; then
				found_AP=true
				break
			fi
			i=$((${i} + 1))
		done
		if ! ${found_AP} ; then
			SSID=${e}
			iwconfig_associate && return 0
		fi
	done

	ewarn "Failed to associate with any preferred access points on ${IFACE}"
	return 1
}

iwconfig_connect_preferred() {
	local essid= i=0 mode= mac= caps= freq= chan=

	eval "$(_get_array preferred_aps)"
	for essid in "$@"; do
		while [ ${i} -le ${APS} ]  ; do
			eval e=\$SSID_${i}
			if [ "${e}" = "${essid}" ] ; then
				SSID=${e}
				eval mac=\$MAC_${i}
				eval caps=\$CAPS_${i}
				eval freq=\$FREQ_${i}
				eval chan=\$CHAN_${i}
				iwconfig_associate "${mac}" \
				"${chan}" "${caps}" && return 0
			fi
			i=$((${i} + 1))
		done
	done

	return 1
}

iwconfig_connect_not_preferred() {
	local essid= i=0 mode= mac= caps= freq= chan= pref=

	while [ ${i} -le ${APS} ] ; do
		eval e=\$SSID_${i}
		if [ -z "${e}" ] ; then
		    i=$((${i} + 1))
		    continue
		fi

		eval "$(_get_array preferred_aps)"
		pref=false
		for essid in "$@" ; do
			if [ "${e}" = "${essid}" ] ; then
				pref=true
				break
			fi
		done

		if ! ${pref} ; then
			SSID=${e}
			eval mac=\$MAC_${i}
			eval caps=\$CAPS_${i}
			eval freq=\$FREQ_${i}
			eval chan=\$CHAN_${i}
			iwconfig_associate "${mac}"  \
				"${chan}" "${caps}" && return 0
		fi
		i=$((${i} + 1))
	done

	return 1
}

iwconfig_defaults() {
	# Set some defaults
	#ifconfig "${iface}" txpower 100 2>/dev/null
	ifconfig "${IFACE}" bssid -
	ifconfig "${IFACE}" ssid -
	ifconfig "${IFACE}" authmode open
	ifconfig "${IFACE}" -mediaopt adhoc
	ifconfig "${IFACE}" -mediaopt hostap
}

iwconfig_configure() {
	local x APS
	eval SSID=\$ssid_${IFVAR}

	# Setup ad-hoc mode?
	eval x=\$mode_${IFVAR}
	x=${x:-managed}
	case "${x}" in
	    ad-hoc|adhoc|hostap|master) iwconfig_setup_specific "${x}" ;;
	esac

	if [ "${x}" != "managed" -a "${x}" != "auto" ] ; then
		eerror "Only managed, ad-hoc, master and auto modes are supported"
		return 1
	fi

	# Has an ESSID been forced?
	if [ -n "${SSID}" ]; then
		iwconfig_set_mode "${x}"
		iwconfig_associate && return 0
		[ "${SSID}" = "any" ] && iwconfig_force_preferred && return 0

		eval SSID=\$adhoc_ssid_${IFVAR}
		if [ -n "${SSID}" ]; then
			iwconfig_setup_specific adhoc
			return $?
		fi
		return 1
	fi

	# Are we forcing preferred only?
	eval x=\$associate_order_${IFVAR}
	[ -n "${x}" ] && associate_order=${x}
	associate_order=${associate_order:-any}
	if [ "${associate_order}" = "forcepreferredonly" ]; then
		iwconfig_force_preferred && return 0
	else
		iwconfig_scan || return 1
		iwconfig_connect_preferred && return 0
		[ "${associate_order}" = "forcepreferred" ] || \
		[ "${associate_order}" = "forceany" ] && \
		iwconfig_force_preferred && return 0
		[ "${associate_order}" = "any" ] || \
		[ "${associate_order}" = "forceany" ] && \
		iwconfig_connect_not_preferred && return 0
	fi

	e="associate with"
	[ -z "${MAC_0}" ] && e="find"
	[ "${preferred_aps}" = "force" ] || \
	[ "${preferred_aps}" = "forceonly" ] && \
	e="force"
	e="Couldn't ${e} any access points on ${IFACE}"

	eval SSID=\$adhoc_ssid_${IFVAR}
	if [ -n "${SSID}" ]; then
		ewarn "${e}"
		iwconfig_setup_specific adhoc
		return $?
	fi

	eerror "${e}"
	return 1
}

iwconfig_pre_start() {
	# We don't configure wireless if we're being called from
	# the background
	${IN_BACKGROUND} && return 0

	save_options "SSID" ""
	_exists || return 0

	if ! _is_wireless ; then
		veinfo "${IFACE} is not wireless"
		return 0
	fi

	iwconfig_defaults 
	iwconfig_user_config 
	
	# Set the base metric to be 2000
	metric=2000

	einfo "Configuring wireless network for ${IFACE}"

	if iwconfig_configure ; then
		save_options "ESSID" "${ESSID}"
		return 0
	fi

	eerror "Failed to configure wireless for ${IFACE}"
	iwconfig_defaults
	#iwconfig "${IFACE}" txpower 0 2>/dev/null
	unset SSID SSIDVAR
	_down
	return 1
}

iwconfig_post_stop() {
	${IN_BACKGROUND} && return 0
	_is_wireless || return 0
	iwconfig_defaults
	#iwconfig "${IFACE}" txpower 0 2>/dev/null
}

# vim: set ts=4 
