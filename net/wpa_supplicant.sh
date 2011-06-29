# Copyright (c) 2007-2009 Roy Marples <roy@marples.name>
# Released under the 2-clause BSD license.

wpa_supplicant_depend()
{
	wpas=/usr/sbin/wpa_supplicant
	[ -x ${wpas} ] || wpas=/sbin/wpa_supplicant
	if [ -x ${wpas} ]; then
		program start ${wpas}
		# bug 345281: if wpa_supplicant is built w/ USE=dbus, we need to start
		# dbus before we can start wpa_supplicant.
		${wpas} -h |grep DBus -sq
		[ $? -eq 0 ] && need dbus
	fi
	after macnet plug
	before interface
	provide wireless

	# Prefer us over iwconfig
	after iwconfig
}

# Only set these functions if not set already
# IE, prefer to use iwconfig
if ! type _get_ssid >/dev/null 2>&1; then
_get_ssid()
{
	local timeout=5 ssid=

	while [ ${timeout} -gt 0 ]; do
		ssid=$(wpa_cli -i"${IFACE}" status | sed -n -e 's/^ssid=//p')
		if [ -n "${ssid}" ]; then
			echo "${ssid}"
			return 0
		fi
		sleep 1
		timeout=$((timeout - 1))
	done

	return 1
}

_get_ap_mac_address()
{
	wpa_cli -i"${IFACE}" status | sed -n -e 's/^bssid=\(.*\)$/\1/p' \
		| tr '[:lower:]' '[:upper:]'
}
fi

wpa_supplicant_pre_start()
{
	local opts= cliopts= cfgfile= ctrl_dir= wireless=true
	local wpas=/usr/sbin/wpa_supplicant wpac=/usr/bin/wpa_cli
	local actfile=/etc/wpa_supplicant/wpa_cli.sh

	if [ ! -x "${wpas}" ]; then
		wpas=/sbin/wpa_supplicant
		wpac=/bin/wpa_cli
	fi
	[ "${RC_UNAME}" = "Linux" ] || unset wpac
	[ -e "${actfile}" ] || unset wpac

	eval opts=\$wpa_supplicant_${IFVAR}
	eval cliopts=\$wpa_cli_${IFVAR}
	[ -z "${cliopts}" ] && cliopts=${wpa_cli}
	case " ${opts} " in
		*" -Dwired "*) wireless=false;;
		*) _is_wireless || return 0;;
	esac

	# We don't configure wireless if we're being called from
	# the background unless we're not currently running
	if yesno ${IN_BACKGROUND}; then
		if ${wireless} && \
		service_started_daemon "${RC_SVCNAME}" "${wpas}"; then
			SSID=$(_get_ssid "${IFACE}")
			SSIDVAR=$(shell_var "${SSID}")
			service_set_value "SSID" "${SSID}"
			metric=2000
		fi
		return 0
	fi

	service_set_value "SSID" ""
	ebegin "Starting wpa_supplicant on ${IFVAR}"

	if type iwconfig_defaults >/dev/null 2>&1; then
		iwconfig_defaults
		iwconfig_user_config
	fi

	cfgfile=${opts##* -c}
	if [ -n "${cfgfile}" -a "${cfgfile}" != "${opts}" ]; then
		case "${cfgfile}" in
			" "*) cfgfile=${cfgfile# *};;
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

	if [ ! -f ${cfgfile} ]; then
		eend 1 "/etc/wpa_supplicant/wpa_supplicant.conf not found"
		return 1
	fi

	# Work out where the ctrl_interface dir is if it's not specified
	local ctrl_dir=$(sed -e 's/^ *//' \
				-e '/^ctrl_interface=/!d' \
				-e 's/^ctrl_interface=//' \
				-e 's/^ *//' \
				-e 's/^DIR=//' \
				-e 's/^ *//' \
				-e 's/GROUP=.*//' \
				-e 's/ *$//' \
				"${cfgfile}")
	if [ -z "${ctrl_dir}" ]; then
		ctrl_dir=${opts##* -C}
		if [ -n "${ctrl_dir}" -a "${ctrl_dir}" != "${opts}" ]; then
			case "${ctrl_dir}" in
				" "*) ctrl_dir=${ctrl_dir# *};;
			esac
			ctrl_dir=${ctrl_dir%% *}
		else
			ctrl_dir="/var/run/wpa_supplicant"
			opts="${opts} -C ${ctrl_dir}"
		fi
	fi
	service_set_value ctrl_dir "${ctrl_dir}"

	if [ -n "${wpac}" ]; then
		opts="${opts} -W"
	elif service_started devd; then
		mark_service_inactive
	fi
	start-stop-daemon --start --exec "${wpas}" \
		--pidfile "/var/run/wpa_supplicant-${IFACE}.pid" \
		-- ${opts} -B -i "${IFACE}" \
		-P "/var/run/wpa_supplicant-${IFACE}.pid"
	eend $? || return 1

	# If we don't have a working wpa_cli and action file continue
	if [ -z "${wpac}" ]; then
		if service_started devd; then
			ebegin "Backgrounding ..."
			exit 1
		fi
		return 0
	fi

	# Starting wpa_supplication-0.4.0, we can get wpa_cli to
	# start/stop our scripts from wpa_supplicant messages
	local inact=false
	service_inactive && inact=true
	mark_service_inactive

	ebegin "Starting wpa_cli on" "${IFACE}"
	start-stop-daemon --start --exec "${wpac}" \
		--pidfile "/var/run/wpa_cli-${IFACE}.pid" \
		-- ${cliopts} -a "${actfile}" -p "${ctrl_dir}" -i "${IFACE}" \
		-P "/var/run/wpa_cli-${IFACE}.pid" -B
	if eend $?; then
		ebegin "Backgrounding ..."
		exit 1
	fi

	# wpa_cli failed to start? OK, error here
	start-stop-daemon --quiet --stop --exec "${wpas}" \
		--pidfile "/var/run/wpa_supplicant-${IFACE}.pid"
	${inact} ||	mark_service_stopped
	return 1
}

wpa_supplicant_post_stop()
{
	local wpas=/usr/sbin/wpa_supplicant wpac=/usr/bin/wpa_cli

	if [ ! -x "${wpas}" ]; then
		wpas=/sbin/wpa_supplicant
		wpac=/bin/wpa_cli
	fi

	if yesno "${IN_BACKGROUND}"; then
		# Only stop wpa_supplicant if it's not the controlling daemon
		! service_started_daemon "${RC_SVCNAME}" "${wpas}" 1
	fi
	[ $? != 0 ] && return 0

	local pidfile="/var/run/wpa_cli-${IFACE}.pid"
	if [ -f ${pidfile} ]; then
		ebegin "Stopping wpa_cli on ${IFACE}"
		start-stop-daemon --stop --exec "${wpac}" --pidfile "${pidfile}"
		eend $?
	fi

	pidfile="/var/run/wpa_supplicant-${IFACE}.pid"
	if [ -f ${pidfile} ]; then
		ebegin "Stopping wpa_supplicant on ${IFACE}"
		start-stop-daemon --stop --exec "${wpas}" --pidfile "${pidfile}"
		eend $?
	fi

	# If wpa_supplicant exits uncleanly, we need to remove the stale dir
	[ -S "/var/run/wpa_supplicant/${IFACE}" ] \
		&& rm -f "/var/run/wpa_supplicant/${IFACE}"
}
