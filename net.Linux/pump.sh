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

pump_depend() {
	program /sbin/pump
	after interface
	provide dhcp
}

_config_vars="$_config_vars dhcp pump"

pump_start() {
	local args= opt= opts=

	# Get our options
	eval opts=\$dhcp_${IFVAR}
	[ -z "${opts}" ] && opts=${dhcp}

	# Map some generic options to dhcpcd
	for opt in ${opts}; do
		case "${opt}" in
			nodns) args="${args} --no-dns";;
			nontp) args="${args} --no-ntp";;
			nogateway) args="${args} --no-gateway";;
		esac
	done

	# Add our route metric
	[ "${metric:-0}" != "0" ] && args="${args} --route-metric ${metric}"

	args="${args} --win-client-ident"
	args="${args} --keep-up --interface ${IFACE}"

	ebegin "Running pump"
	eval pump "${args}"
	eend $? || return 1 

	_show_address
	return 0
}

pump_stop() {
	# We check for a pump process first as querying for status
	# causes pump to spawn a process
	start-stop-daemon --quiet --test --stop --exec /sbin/pump || return 0

	# Check that pump is running on the interface
	if ! pump --status --interface "${IFACE}" >/dev/null 2>&1; then 
		return 0
	fi

	# Pump always releases the lease
	ebegin "Stopping pump on ${IFACE}"
	pump --release --interface "${IFACE}"
	eend $? "Failed to stop pump"
}

# vim: set ts=4 :
