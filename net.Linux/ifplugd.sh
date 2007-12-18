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

_config_vars="$_config_vars plug_timeout"

ifplugd_depend() {
	program start /usr/sbin/ifplugd
	after macnet rename
	before interface
	provide plug
}

ifplugd_pre_start() {
	local pidfile="/var/run/ifplugd.${IFACE}.pid" timeout= args=

	# We don't start ifplugd if we're being called from the background
	yesno ${IN_BACKGROUND} && return 0

	_exists || return 0

	# We need a valid MAC address
	# It's a basic test to ensure it's not a virtual interface
	if ! _get_mac_address >/dev/null 2>&1; then
		vewarn "ifplugd only works on interfaces with a valid MAC address"
		return 0
	fi

	# We don't work on bonded, bridges, tun/tap, vlan or wireless
	for f in bond bridge tuntap vlan wireless; do
		if type "_is_${f}" >/dev/null 2>&1; then
			if _is_${f}; then
				veinfo "netplug does not work with" "${f}"
				return 0
			fi
		fi
	done

	ebegin "Starting ifplugd on" "${IFACE}"

	eval args=\$ifplugd_${IFVAR}

	# Mark the us as inactive so netplug can restart us
	mark_service_inactive

	# Start ifplugd
	eval start-stop-daemon --start --exec /usr/sbin/ifplugd \
		--pidfile "${pidfile}" -- "${args}" --iface="${IFACE}"
	eend "$?" || return 1

	eindent

	eval timeout=\$plug_timeout_${IFVAR}
	[ -z "${timeout}" ] && timeout=-1
	if [ ${timeout} -eq 0 ]; then
		ewarn "WARNING: infinite timeout set for ${IFACE} to come up"
	elif [ ${timeout} -lt 0 ]; then
		einfo "Backgrounding ..."
		exit 1 
	fi

	veinfo "Waiting for ${IFACE} to be marked as started"

	local i=0
	while true; do
		if service_started; then
			_show_address
			exit 0
		fi
		sleep 1
		[ ${timeout} -eq 0 ] && continue
		i=$((${i} + 1))
		[ ${i} -ge ${timeout} ] && break
	done

	eend 1 "Failed to configure ${IFACE} in the background"
	exit 1
}

ifplugd_stop() {
	yesno ${IN_BACKGROUND} && return 0

	local pidfile="/var/run/ifplugd.${IFACE}.pid"
	[ ! -e "${pidfile}" ] && return 0
	
	ebegin "Stopping ifplugd on" "${IFACE}"
	start-stop-daemon --stop --quiet --exec /usr/sbin/ifplugd \
		--pidfile "${pidfile}" --signal QUIT
	eend $?
}

# vim: set ts=4 :
