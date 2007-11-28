# Copyright 2004-2007 Gentoo Foundation
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

udhcpc_depend() {
	program start /sbin/udhcpc
	after interface
	provide dhcp
}

_config_vars="$_config_vars dhcp udhcpc"

udhcpc_start() {
	local args= opt= opts= pidfile="/var/run/udhcpc-${IFACE}.pid"
	local sendhost=true cachefile="/var/cache/udhcpc-${IFACE}.lease"

	eval args=\$udhcpc_${IFVAR}

	# Get our options
	eval opts=\$dhcp_${IFVAR}
	[ -z "${opts}" ] && opts=${dhcp}

	# Map some generic options to dhcpcd
	for opt in ${opts}; do
		case "${opt}" in
			nodns) args="${args} --env PEER_DNS=no";;
			nontp) args="${args} --env PEER_NTP=no";;
			nogateway) args="${args} --env PEER_ROUTERS=no";;
			nosendhost) sendhost=false;
		esac
	done

	[ "${metric:-0}" != "0" ] && args="${args} --env IF_METRIC=${metric}"

	ebegin "Running udhcpc"

	# Try and load the cache if it exists
	if [ -f "${cachefile}" ]; then
		case "$ {args} " in
			*" --request="*|*" -r "*);;
			*)
				local x=$(cat "${cachefile}")
				# Check for a valid ip
				case "${x}" in
					*.*.*.*) args="${args} --request=${x}";;
				esac
				;;
		esac
	fi

	case " ${args} " in
		*" --quit "*|*" -q "*) x="/sbin/udhcpc";;
		*) x="start-stop-daemon --start --exec /sbin/udhcpc \
			--pidfile \"${pidfile}\" --";;
	esac

	case " ${args} " in
		*" --hosname="*|*" -h "*|*" -H "*);;
		*)
			if ${sendhost}; then
				local hname="$(hostname)"
				if [ "${hname}" != "(none)" ] && [ "${hname}" != "localhost" ]; then
					args="${args} --hostname='${hname}'"
				fi
			fi
			;;
	esac

	local script="${RC_LIBDIR}"/sh/udhcpc.sh
	[ -x "${script}" ] || script=/lib/rcscripts/sh/udhcpc.sh

	eval "${x}" "${args}" --interface="${IFACE}" --now \
		--script="${script}" \
		--pidfile="${pidfile}" >/dev/null
	eend $? || return 1

	_show_address
	return 0
}

udhcpc_stop() {
	local pidfile="/var/run/udhcpc-${IFACE}.pid" opts=
	[ ! -f "${pidfile}" ] && return 0

	# Get our options
	eval opts=\$dhcp_${IFVAR}
	[ -z "${opts}" ] && opts=${dhcp}

	ebegin "Stopping udhcpc on ${IFACE}"
	case " ${opts} " in
		*" release "*)
			start-stop-daemon --stop --quiet --oknodo --signal USR2 \
				--exec /sbin/udhcpc --pidfile "${pidfile}"
			if [ -f /var/cache/udhcpc-"${IFACE}".lease ]; then
				rm -f /var/cache/udhcpc-"${IFACE}".lease
			fi
			;;
	esac

	start-stop-daemon --stop --exec /sbin/udhcpc --pidfile "${pidfile}"
	eend $?
}

# vim: set ts=4 :
