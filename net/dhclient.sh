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

dhclient_depend() {
	after interface
	program start /sbin/dhclient
	provide dhcp
}

_config_vars="$_config_vars dhcp dhcpcd"

dhclient_start() {
	local args= opt= opts= pidfile="/var/run/dhclient-${IFACE}.pid"
	local sendhost=true dconf=

	# Get our options
	eval opts=\$dhcp_${IFVAR}
	[ -z "${opts}" ] && opts=${dhcp}

	# Map some generic options to dhcpcd
	for opt in ${opts} ; do
		case "${opt}" in
			nodns) args="${args} -e PEER_DNS=no" ;;
			nontp) args="${args} -e PEER_NTP=no" ;;
			nogateway) args="${args} -e PEER_ROUTERS=no" ;;
			nosendhost) sendhost=false ;;
		esac
	done

	# Add our route metric
	[ "${metric:-0}" != "0" ] && args="${args} -e IF_METRIC=${metric}"

	if ${sendhost} ; then
		local hname="$(hostname)"
		if [ "${hname}" != "(none)" -a "${hname}" != "localhost" ]; then
			dhconf="${dhconf} interface \"${IFACE}\" {"
			dhconf="${dhconf} send host-name \"${hname}\";"
			dhconf="${dhconf}}"
		fi
	fi

	# Bring up DHCP for this interface
	ebegin "Running dhclient"
	echo "${dhconf}" | start-stop-daemon --start --exec /sbin/dhclient \
		--pidfile "${pidfile}" -- ${args} -q -1 -pf "${pidfile}" "${IFACE}"
	eend $? || return 1

	_show_address
	return 0
}

dhclient_stop() {
	local pidfile="/var/run/dhclient-${IFACE}.pid" opts=
	[ ! -f "${pidfile}" ] && return 0

	# Get our options
	if [ -x /sbin/dhclient ] ; then
		eval opts=\$dhcp_${IFVAR}
		[ -z "${opts}" ] && opts=${dhcp}
	fi

	ebegin "Stopping dhclient on ${IFACE}"
	case " ${opts} " in
		*" release "*) dhclient -q -r -pf "${pidfile}" "${IFACE}" ;;
		*)
			start-stop-daemon --stop --quiet \
				--exec /sbin/dhclient --pidfile "${pidfile}"
			;;
	esac
	eend $?
}

# vim: set ts=4 :
