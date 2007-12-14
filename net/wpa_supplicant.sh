# Copyright 2007 Roy Marples
# All rights reserved

# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.

wpa_supplicant_depend() {
	if [ -x /usr/sbin/wpa_supplicant ]; then
		program start /usr/sbin/wpa_supplicant
	else
		program start /sbin/wpa_supplicant
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
_get_ssid() {
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

_get_ap_mac_address() {
	wpa_cli -i"${IFACE}" status | sed -n -e 's/^bssid=\(.*\)$/\1/p' \
		| tr '[:lower:]' '[:upper:]'
}
fi

wpa_supplicant_pre_start() {
	local opts= cfgfile= ctrl_dir= wireless=true
	local wpas=/usr/sbin/wpa_supplicant wpac=/usr/bin/wpa_cli

	if [ ! -x "${wpas}" ]; then
		wpas=/sbin/wpa_supplicant
		wpac=/bin/wpa_cli
	fi
	[ "${RC_UNAME}" = "Linux" ] || unset wpac

	eval opts=\$wpa_supplicant_${IFVAR}
	case " ${opts} " in
		*" -Dwired "*) wireless=false;;
		*) _is_wireless || return 0;;
	esac

	# We don't configure wireless if we're being called from
	# the background unless we're not currently running
	if yesno ${IN_BACKGROUND}; then
		if ${wireless} && service_started_daemon "${SVCNAME}" "${wpas}"; then
			SSID=$(_get_ssid "${IFACE}")
			SSIDVAR=$(_shell_var "${SSID}")
			save_options "SSID" "${SSID}"
			metric=2000
		fi
		return 0
	fi

	save_options "SSID" ""
	ebegin "Starting wpa_supplicant on ${IFVAR}"

	if [ -x /sbin/iwconfig ]; then
		local x=
		for x in txpower rate rts frag; do
			iwconfig "${IFACE}" "${x}" auto 2>/dev/null
		done
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
	local ctrl_dir=$(sed -n -e 's/[[:space:]]*#.*//g;s/[[:space:]]*$//g;s/^ctrl_interface=//p' "${cfgfile}")
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
	save_options ctrl_dir "${ctrl_dir}"

	actfile="/etc/wpa_supplicant/wpa_cli.sh"

	if [ -n "${wpac}" ]; then
		opts="${opts} -W"
	else
		sleep 2 # FBSD 7.0 beta2 bug
		mark_service_inactive "${SVCNAME}"
	fi
	start-stop-daemon --start --exec "${wpas}" \
		--pidfile "/var/run/wpa_supplicant-${IFACE}.pid" \
		-- ${opts} -B -i "${IFACE}" \
		-P "/var/run/wpa_supplicant-${IFACE}.pid"
	eend $? || return 1
	if [ -z "${wpac}" ]; then
		ebegin "Backgrounding ..."
		exit 1 
	fi

	# Starting wpa_supplication-0.4.0, we can get wpa_cli to
	# start/stop our scripts from wpa_supplicant messages
	local inact=false
	service_inactive "${SVCNAME}" && inact=true
	mark_service_inactive "${SVCNAME}"

	ebegin "Starting wpa_cli on" "${IFACE}"
	start-stop-daemon --start --exec "${wpac}" \
		--pidfile "/var/run/wpa_cli-${IFACE}.pid" \
		-- -a /etc/wpa_supplicant/wpa_cli.sh -p "${ctrl_dir}" -i "${IFACE}" \
		-P "/var/run/wpa_cli-${IFACE}.pid" -B
	if eend $?; then
		ebegin "Backgrounding ..."
		exit 1 
	fi

	# wpa_cli failed to start? OK, error here
	start-stop-daemon --quiet --stop --exec "${wpas}" \
		--pidfile "/var/run/wpa_supplicant-${IFACE}.pid"
	${inact} ||	mark_service_stopped "${SVCNAME}"
	return 1
}

wpa_supplicant_post_stop() {
	local wpas=/usr/sbin/wpa_supplicant wpac=/usr/bin/wpa_cli

	if [ ! -x "${wpas}" ]; then
		wpas=/sbin/wpa_supplicant
		wpac=/bin/wpa_cli
	fi

	if yesno "${IN_BACKGROUND}"; then
		# Only stop wpa_supplicant if it's not the controlling daemon
		! service_started_daemon "${SVCNAME}" "${wpas}" 1
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

# vim: set ts=4 :
