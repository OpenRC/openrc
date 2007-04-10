# Copyright 2004-2007 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# Many thanks to all the people in the Gentoo forums for their ideas and
# motivation for me to make this and keep on improving it

_config_vars="$_config_vars ssid mode associate_timeout sleep_scan preferred_aps blacklist_aps"

iwconfig_depend() {
	program /sbin/iwconfig
	after plug
	before interface
	provide wireless
}

iwconfig_get_wep_status() {
	local mode= status="disabled"

	# No easy way of doing this grep in bash regex :/
	if LC_ALL=C iwconfig "${IFACE}" | grep -qE "^ +Encryption key:[*0-9,A-F]" ; then
		status="enabled"
		mode=$(LC_ALL=C iwconfig "${IFACE}" | sed -n -e 's/^.*Security mode:\(.*[^ ]\).*/\1/p')
		[ -n "${mode}" ] && mode=" - ${mode}"
	fi

	echo "(WEP ${status}${mode})"
}

_get_ssid() {
	local i=5 ssid=

	while [ ${i} -gt 0 ] ; do
		ssid=$(iwgetid --raw "${IFACE}")
		if [ -n "${ssid}" ] ; then
			echo "${ssid}"
			return 0
		fi
		sleep 1
		i=$((${i} + 1))
	done

	return 1
}

_get_ap_mac_address() {
	local mac="$(iwgetid --raw --ap "${IFACE}")" 
	case "${mac}" in
		"00:00:00:00:00:00") return 1 ;;
		"44:44:44:44:44:44") return 1 ;;
		"FF:00:00:00:00:00") return 1 ;;
		"FF:FF:FF:FF:FF:FF") return 1 ;;
		*) echo "${mac}" ;;
	esac
}

iwconfig_get_mode() {
	LC_ALL=C iwgetid --mode "${IFACE}" | \
	sed -n -e 's/^.*Mode:\(.*\)/\1/p' | \
	tr '[:upper:]' '[:lower:]'
}

iwconfig_set_mode() {
	local mode="$1"
	[ "${mode}" = "$(iwconfig_get_mode)" ] && return 0

	# Devicescape stack requires the interface to be down
	_down
	iwconfig "${IFACE}" mode "${mode}" || return 1
	_up
}

iwconfig_get_type() {
	LC_ALL=C iwconfig "${IFACE}" | sed -n -e 's/^'"$1"' *\([^ ]* [^ ]*\).*/\1/p'
}

iwconfig_report() {
	local mac= m="connected to"
	local ssid="$(_get_ssid)"
	local wep_status="$(iwconfig_get_wep_status)"
	local channel="$(iwgetid --raw --channel "${iface}")"
	[ -n "${channel}" ] && channel="on channel ${channel} "
	local mode="$(iwconfig_get_mode)"
	if [ "${mode}" = "master" ]; then
		m="configured as"
	else
		mac="$(_get_ap_mac_address)"
		[ -n "${mac}" ] && mac=" at ${mac}"
	fi

	eindent
	einfo "${IFACE} ${m} SSID \"${SSID}\"${mac}"
	einfo "in ${mode} mode ${channel}${wep_status}"
	eoutdent
}

iwconfig_get_wep_key() {
	local mac="$1" key=
	[ -n "${mac}" ] && mac="$(echo "${mac}" | sed -e 's/://g')"
	eval key=\$mac_key_${mac}
	[ -z "${key}" ] && eval key=\$key_${SSIDVAR}
	if [ -z "${key}" ] ; then
		echo "off"
	else
		set -- ${key}
		local x= e=false
		for x in "$@" ; do
			if [ "${x}" = "enc" ] ; then
				e=true
				break
			fi
		done
		${e} || key="${key} enc open"
		echo "${key}"
	fi
}

iwconfig_user_config() {
	local conf= var=${SSIDVAR}
	[ -z "${var}" ] && var=${IFVAR}

	eval "$(_get_array "iwconfig_${var}")"
	for conf in "$@" ; do
		if ! eval iwconfig "${IFACE}" "${conf}" ; then
			ewarn "${IFACE} does not support the following configuration commands"
			ewarn "  ${conf}"
		fi
	done

	eval "$(_get_array "iwpriv_${var}")"
	for conf in "$@" ; do
		if ! eval iwpriv "${IFACE}" "${conf}" ; then
			ewarn "${IFACE} does not support the following private ioctls"
			ewarn "  ${conf}"
		fi
	done
}

iwconfig_setup_specific() {
	local mode="$1" channel=
	if [ -z "${SSID}" ]; then
		eerror "${IFACE} requires an SSID to be set to operate in ${mode} mode"
		eerror "adjust the ssid_${IFVAR} setting in /etc/conf.d/net"
		return 1
	fi
	SSIDVAR=$(_shell_var "${SSID}")
	local key=$(iwconfig_get_wep_key)

	iwconfig_set_mode "${mode}"
	
	# Now set the key
	if ! eval iwconfig "${IFACE}" key "${key}" ; then
		if [ "${key}" != "off" ]; then
			ewarn "${IFACE} does not support setting keys"
			ewarn "or the parameter \"mac_key_${SSIDVAR}\" or \"key_${SSIDVAR}\" is incorrect"
		fi
	fi

	# Then set the SSID
	if ! iwconfig "${IFACE}" essid "${SSID}" ; then
		eerror "${IFACE} does not support setting SSID to \"${SSID}\""
		return 1
	fi

	eval channel=\$channel_${SSIDVAR}
	[ -z "${channel}" ] && eval channel=\$channel_${IFVAR}
	# We default the channel to 3
	if ! iwconfig "${IFACE}" channel "${channel:-3}" ; then
		ewarn "${IFACE} does not support setting the channel to \"${channel:-3}\""
		return 1
	fi
	
	# Finally apply the user Config
	iwconfig_user_config
	
	iwconfig_report
	return 0
}

iwconfig_wait_for_association() {
	local timeout= i=0
	eval timeout=\$associate_timeout_${IFVAR}
	timeout=${timeout:-10}

	[ ${timeout} -eq 0 ] \
		&& vewarn "WARNING: infinite timeout set for association on ${IFACE}"

	while true; do
		# Use sysfs if we can
		if [ -e /sys/class/net/"${IFACE}"/carrier ] ; then
			if [ "$(cat /sys/class/net/"${IFACE}"/carrier)" = "1" ] ; then
				# Double check we have an ssid. This is mainly for buggy
				# prism54 drivers that always set their carrier on :/
				[ -n "$(iwgetid --raw "${IFACE}")" ] && return 0
			fi
		else
			local atest=
			eval atest=\$associate_test_${IFVAR}
			atest=${atest:-mac}
			if [ "${atest}" = "mac" -o "${atest}" = "all" ] ; then
				[ -n "$(_get_ap_mac_address)" ] && return 0
			fi
			if [ "${atest}" = "quality" -o "${atest}" = "all" ] ; then
				[ "$(sed -n -e 's/^.*'"${IFACE}"': *[0-9]* *\([0-9]*\).*/\1/p' \
					/proc/net/wireless)" != "0" ] && return 0
			fi
		fi
		
		sleep 1
		[ ${timeout} -eq 0 ] && continue
		i=$((${i} +  1))
		[ ${i} -ge ${timeout} ] && return 1
	done
	return 1
}

iwconfig_associate() {
	local mode="${1:-managed}" mac="$2" wep_required="$3" freq="$4" chan="$5"
	local w="(WEP Disabled)" key=

	iwconfig_set_mode "${mode}"

	if [ "${SSID}" = "any" ]; then
		iwconfig "${IFACE}" ap any 2>/dev/null
		unset SSIDVAR
	else
		SSIDVAR=$(_shell_var "${SSID}")
		key="$(iwconfig_get_wep_key "${mac}")"
		if [ "${wep_required}" = "on" -a "${key}" = "off" ] ; then
			ewarn "WEP key is not set for \"${SSID}\" - not connecting"
			return 1
		fi
		if [ "${wep_required}" = "off" -a "${key}" != "off" ] ; then
			key="off"
			ewarn "\"${SSID}\" is not WEP enabled - ignoring setting"
		fi

		if ! eval iwconfig "${IFACE}" key "${key}" ; then
			if [ "${key}" != "off" ] ; then
				ewarn "${IFACE} does not support setting keys"
				ewarn "or the parameter \"mac_key_${SSIDVAR}\" or \"key_${SSIDVAR}\" is incorrect"
				return 1
			fi
		fi
		[ "${key}" != "off" ] && w="$(iwconfig_get_wep_status "${iface}")"
	fi

	if ! iwconfig "${IFACE}" essid "${SSID}" ; then
		if [ "${SSID}" != "any" ] ; then
			ewarn "${IFACE} does not support setting SSID to \"${SSID}\""
		fi
	fi

	# Only use channel or frequency
	if [ -n "${chan}" ] ; then
		iwconfig "${IFACE}" channel "${chan}"
	elif [ -n "${freq}" ] ; then
		iwconfig "${IFACE}" freq "${freq}"
	fi
	[ -n "${mac}" ] && iwconfig "${IFACE}" ap "${mac}"
	
	# Finally apply the user Config
	iwconfig_user_config

	ebegin "Connecting to \"${SSID}\" in ${mode} mode ${w}"

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

	if ! iwconfig_wait_for_association  ; then
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
	local x= i=0 scan=
	einfo "Scanning for access points"
	eindent

	# Sleep if required
	eval x=\$sleep_scan_${IFVAR}
	[ -n "${x}" ] && sleep "${x}"

	while [ ${i} -lt 3 ] ; do
	    scan="${scan} $(iwlist "${IFACE}" scan 2>/dev/null | sed -e "s/'/'\\\\''/g" -e "s/$/'/g" -e "s/^/'/g")"
	    i=$((${i} + 1))
	done

	if [ -z "${scan}" ] ; then
		ewarn "${iface} does not support scanning"
		eoutdent
		eval x=\$adhoc_ssid_${IFVAR}
		[ -n "${x}" ] && return 0
		if [ -n "${preferred_aps}" ] ; then
			[ "${associate_order}" = "forcepreferred" ] || \
			[ "${associate_order}" = "forcepreferredonly" ] && return 0
		fi
		eerror "You either need to set a preferred_aps list in /etc/conf.d/wireless"
		eerror "   preferred_aps=\"SSID1 SSID2\""
		eerror "   and set associate_order_${IFVAR}=\"forcepreferred\""
		eerror "   or set associate_order_${IFVAR}=\"forcepreferredonly\""
		eerror "or hardcode the  SSID to \"any\" and let the driver find an Access Point"
		eerror "   ssid_${IFVAR}=\"any\""
		eerror "or configure defaulting to Ad-Hoc when Managed fails"
		eerror "   adhoc_ssid_${IFVAR}=\"WLAN\""
		eerror "or hardcode the SSID against the interface (not recommended)"
		eerror "   ssid_${IFVAR}=\"SSID\""
		return 1
	fi

	local OIFS=$IFS
	APS=-1
	eval set -- ${scan}
	for line in "$@" ; do
	    case "${line}" in
		*Address:*)
		    APS=$((${APS} + 1))
		    eval MAC_${APS}=\""$(echo "${line#*: }" | tr '[:lower:]' '[:upper:]')"\"
		    eval QUALITY_${APS}=0
		    ;;
		*ESSID:*)
		    x=${line#*\"}
		    x=${x%*\"}
		    eval SSID_${APS}=\$x
		    ;;
		*Mode:*)
		    x="$(echo "${line#*:}" | tr '[:upper:]' '[:lower:]')"
		    if [ "${x}" = "master" ] ; then
			eval MODE_${APS}=\"managed\"
		    else
			eval MODE_${APS}=\$x
		    fi
		    ;;
		*'Encryption key:'*)
		    x=${line#*:}
		    eval ENC_${APS}=\$x
		    ;;
		#*Frequency:*)
		#		freq[i]="${line#*:}"
		#		x="${freq[i]#* }"
		#		freq[i]="${freq[i]%% *}${x:0:1}"
		#		;;
		*Channel:*)
		    x=${line#*:}
		    x=${x%% *}
		    eval CHAN_${APS}=\$x
		    ;;
		*Quality*)
		    x=${line#*:}
		    x=${x%/*}
		    x="$(echo "${x}" | sed -e 's/[^[:digit:]]//g')"
		    x=${x:-0}
		    eval QUALITY_${APS}=\$x
		    ;;
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
		    for x in MAC SSID CHAN QUALITY ENC ; do
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
			    unset MAC_${k} SSID_${k} CHAN_${k} QUALITY_${k} ENC_${k}
			else
			    unset MAC_${i} SSID_${i} CHAN_${i} QUALITY_${i} ENC_${i}
			fi
		    else
			unset MAC_${k} SSID_${k} CHAN_${k} QUALITY_${k} ENC_${k}
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
		eval s=\$SSID_${i}
		eval q=\$QUALITY_${i}
		eval e=\$ENC_${i}
		if [ -n "${e}" -a "${e}" != "off" ] ; then
		    e=", encrypted"
		else
		    e=""
		fi
		if [ -z "${s}" ] ; then
			einfo "Found ${x}, ${m}${e}"
		else
			einfo "Found \"${s}\" at ${x}, ${m}${e}"
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
				unset SSID_${i} MAC_${i} CHAN_${i} QUALITY_${i} ENC_${i}
			fi
		done
		i=$((${i} + 1))
	done
	eoutdent
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
	local ssid= i=0 mode= mac= enc= freq= chan=

	eval "$(_get_array preferred_aps)"
	for ssid in "$@"; do
		while [ ${i} -le ${APS} ]  ; do
			eval e=\$SSID_${i}
			if [ "${e}" = "${ssid}" ] ; then
				SSID=${e}
				eval mode=\$MODE_${i}
				eval mac=\$MAC_${i}
				eval enc=\$ENC_${i}
				eval freq=\$FREQ_${i}
				eval chan=\$CHAN_${i}
				iwconfig_associate "${mode}" "${mac}" "${enc}" "${freq}" \
					"${chan}" && return 0
			fi
			i=$((${i} + 1))
		done
	done

	return 1
}

iwconfig_connect_not_preferred() {
	local ssid= i=0 mode= mac= enc= freq= chan= pref=false

	while [ ${i} -le ${APS} ] ; do
		eval e=\$SSID_${i}
		eval "$(_get_array preferred_aps)"
		for ssid in "$@" ; do
			if [ "${e}" = "${ssid}" ] ; then
				pref=true
				break
			fi
		done

		if ! ${pref} ; then
			SSID=${e}
			eval mode=\$MODE_${i}
			eval mac=\$MAC_${i}
			eval enc=\$ENC_${i}
			eval freq=\$FREQ_${i}
			eval chan=\$CHAN_${i}
			iwconfig_associate "${mode}" "${mac}" "${enc}" "${freq}" \
				"${chan}" && return 0
		fi
		i=$((${i} + 1))
	done

	return 1
}

iwconfig_defaults() {
	local x=
	for x in txpower rate rts frag ; do
	    iwconfig "${IFACE}" "${x}" auto 2>/dev/null
	done

	# Release the AP forced
	# Must do ap and then ssid otherwise scanning borks
	iwconfig "${IFACE}" ap off 2>/dev/null
	iwconfig "${IFACE}" essid off 2>/dev/null
}

iwconfig_configure() {
	local x APS
	eval SSID=\$ssid_${IFVAR}

	# Support old variable
	[ -z "${SSID}" ] && eval SSID=\$essid_${IFVAR}

	# Setup ad-hoc mode?
	eval x=\$mode_${IFVAR}
	x=${x:-managed}
	if [ "${x}" = "ad-hoc" -o "${x}" = "master" ] ; then
		iwconfig_setup_specific "${x}"
		return $?
	fi

	if [ "${x}" != "managed" -a "${x}" != "auto" ] ; then
		eerror "Only managed, ad-hoc, master and auto modes are supported"
		return 1
	fi

	# Has an SSID been forced?
	if [ -n "${SSID}" ]; then
		iwconfig_set_mode "${x}"
		iwconfig_associate && return 0
		[ "${SSID}" = "any" ] && iwconfig_force_preferred && return 0

		eval SSID=\$adhoc_ssid_${IFVAR}
		if [ -n "${SSID}" ]; then
			iwconfig_setup_specific ad-hoc
			return $?
		fi
		return 1
	fi

	# Do we have a preferred Access Point list specific to the interface?
#	x="preferred_aps_${ifvar}[@]"
#	[[ -n ${!x} ]] && preferred_aps=( "${!x}" )

#	# Do we have a blacklist Access Point list specific to the interface?
#	x="blacklist_aps_${ifvar}[@]"
#	[[ -n ${!x} ]] && blacklist_aps=( "${!x}" )

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
		iwconfig_setup_specific ad-hoc
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
		veinfo "Wireless extensions not found for ${IFACE}"
		return 0
	fi

	iwconfig_defaults 
	iwconfig_user_config 
	
	# Set the base metric to be 2000
	metric=2000

	# Check for rf_kill - only ipw supports this at present, but other
	# cards may in the future.
	if [ -e /sys/class/net/"${IFACE}"/device/rf_kill ] ; then
		if [ $(cat /sys/class/net/"${IFACE}"/device/rf_kill) != "0" ] ; then
			eerror "Wireless radio has been killed for interface ${IFACE}"
			return 1
		fi
	fi

	einfo "Configuring wireless network for ${IFACE}"

	# Are we a proper IEEE device?
	# Most devices reutrn IEEE 802.11b/g - but intel cards return IEEE
	# in lower case and RA cards return RAPCI or similar
	# which really sucks :(
	# For the time being, we will test prism54 not loading firmware
	# which reports NOT READY!
	x="$(iwconfig_get_type)"
	if [ "${x}" = "NOT READY!" ]; then
		eerror "Looks like there was a probem loading the firmware for ${IFACE}"
		return 1
	fi

	if iwconfig_configure ; then
		save_options "SSID" "${SSID}"
		return 0
	fi

	eerror "Failed to configure wireless for ${IFACE}"
	iwconfig_defaults
	iwconfig "${IFACE}" txpower off 2>/dev/null
	unset SSID SSIDVAR
	_down
	return 1
}

iwconfig_post_stop() {
	${IN_BACKGROUND} && return 0
	_exists || return 0
	iwconfig_defaults
	iwconfig "${IFACE}" txpower off 2>/dev/null
}

# vim: set ts=4 
