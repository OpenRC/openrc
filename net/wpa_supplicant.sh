# Copyright 2004-2007 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2

wpa_supplicant_depend() {
	program start /sbin/wpa_supplicant
	after macnet plug
	before interface
	provide wireless

	# Prefer us over iwconfig
	after iwconfig
}

# Only set these functions if not set already
# IE, prefer to use iwconfig
if ! type _get_ssid >/dev/null 2>/dev/null ; then
_get_ssid() {
	local timeout=5 ssid=

	while [ ${timeout} -gt 0 ] ;do
		ssid=$(wpa_cli -i"${IFACE}" status | sed -n -e 's/^ssid=//p')
		if [ -n "${ssid}" ] ; then
			echo "${ssid}"
			return 0
		fi
		sleep 1
		timeout=$((timeout - 1))
	done

	return 1
}

_get_ap_mac_address() {
	wpa_cli -i"${IFACE}" status | sed -n -e 's/^bssid=\(.*\)$/\1/p' \
		| tr '[:lower:]' '[:upper:]'
}
fi

wpa_supplicant_pre_start() {
	local opts= cfgfile= ctrl_dir=

	_is_wireless || return 0

	# We don't configure wireless if we're being called from
	# the background unless we're not currently running
	if [ "${IN_BACKGROUND}" = "true" ] ; then
		if service_started_daemon "${SVCNAME}" /sbin/wpa_supplicant ; then
			SSID=$(_get_ssid "${IFACE}")
			SSIDVAR=$(_shell_var "${SSID}")
			save_options "SSID" "${SSID}"
			metric=2000
		fi
		return 0
	fi

	save_options "SSID" ""
	eval opts=\$wpa_supplicant_${IFVAR}
	ebegin "Starting wpa_supplicant on" "${IFVAR}"


	if [ -x /sbin/iwconfig ] ; then
		local x=
		for x in txpower rate rts frag ; do
			iwconfig "${IFACE}" "${x}" auto 2>/dev/null
		done
	fi

	cfgfile=${opts##* -c}
	if [ -n "${cfgfile}" -a "${cfgfile}" != "${opts}" ] ; then
		case "${cfgfile}" in
			" "*) cfgfile=${cfgfile# *} ;;
		esac
		cfgfile=${cfgfile%% *}
	else
		# Support new and old style locations
		cfgfile="/etc/wpa_supplicant/wpa_supplicant-${IFACE}.conf"
		[ ! -e "${cfgfile}" ] \
			&& cfgfile="/etc/wpa_supplicant/wpa_supplicant.conf"
		[ ! -e ${cfgfile} ] \
			&& cfgfile="/etc/wpa_supplicant.conf"
		opts="${opts} -c ${cfgfile}"
	fi

	if [ ! -f ${cfgfile} ] ; then
		eend 1 "/etc/wpa_supplicant/wpa_supplicant.conf not found"
		return 1
	fi

	# Work out where the ctrl_interface dir is if it's not specified
	local ctrl_dir=$(sed -n -e 's/[ \t]*#.*//g;s/[ \t]*$//g;s/^ctrl_interface=//p' "${cfgfile}")
	if [ -z "${ctrl_dir}" ] ; then
		ctrl_dir=${opts##* -C}
		if [ -n "${ctrl_dir}" -a "${ctrl_dir}" != "${opts}" ] ; then
			case "${ctrl_dir}" in
				" "*) ctrl_dir=${ctrl_dir# *} ;;
			esac
			ctrl_dir=${ctrl_dir%% *}
		else
			ctrl_dir="/var/run/wpa_supplicant"
			opts="${opts} -C ${ctrl_dir}"
		fi
	fi
	save_options ctrl_dir "${ctrl_dir}"

	actfile="/etc/wpa_supplicant/wpa_cli.sh"

	start-stop-daemon --start --exec /sbin/wpa_supplicant \
		--pidfile "/var/run/wpa_supplicant-${IFACE}.pid" \
		-- ${opts} -W -B -i "${IFACE}" \
		-P "/var/run/wpa_supplicant-${IFACE}.pid"
	eend $? || return 1

	# Starting wpa_supplication-0.4.0, we can get wpa_cli to
	# start/stop our scripts from wpa_supplicant messages
	local inact=false
	service_inactive "${SVCNAME}" && inact=true
	mark_service_inactive "${SVCNAME}"

	ebegin "Starting wpa_cli on" "${IFACE}"
	start-stop-daemon --start --exec /bin/wpa_cli \
		--pidfile "/var/run/wpa_cli-${IFACE}.pid" \
		-- -a /etc/wpa_supplicant/wpa_cli.sh -p "${ctrl_dir}" -i "${IFACE}" \
		-P "/var/run/wpa_cli-${IFACE}.pid" -B
	if eend $? ; then
		ebegin "Backgrounding ..."
		exit 1 
	fi

	# wpa_cli failed to start? OK, error here
	start-stop-daemon --quiet --stop --exec /sbin/wpa_supplicant \
		--pidfile "/var/run/wpa_supplicant-${IFACE}.pid"
	${inact} ||	mark_service_stopped "${SVCNAME}"
	return 1
}

wpa_supplicant_post_stop() {
	if [ "${IN_BACKGROUND}" = "true" ] ; then
		# Only stop wpa_supplicant if it's not the controlling daemon
		! service_started_daemon "${SVCNAME}" /sbin/wpa_supplicant 1 
	fi
	[ $? != 0 ] && return 0

	local pidfile="/var/run/wpa_cli-${IFACE}.pid"
	if [ -f ${pidfile} ] ; then
		ebegin "Stopping wpa_cli on ${IFACE}"
		start-stop-daemon --stop --exec /bin/wpa_cli \
			--pidfile "${pidfile}"
		eend $?
	fi

	pidfile="/var/run/wpa_supplicant-${IFACE}.pid"
	if [ -f ${pidfile} ] ; then
		ebegin "Stopping wpa_supplicant on ${IFACE}"
		start-stop-daemon --stop --exec /sbin/wpa_supplicant \
			--pidfile "${pidfile}"
		eend $?
	fi

	# If wpa_supplicant exits uncleanly, we need to remove the stale dir
	[ -S "/var/run/wpa_supplicant/${IFACE}" ] \
		&& rm -f "/var/run/wpa_supplicant/${IFACE}"
}

# vim: set ts=4 :
