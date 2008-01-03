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

_br2684ctl() {
	if [ -x /usr/sbin/br2684ctl ]; then
		echo /usr/sbin/br2684ctl
	else
		echo /sbin/br2684ctl
	fi
}

br2684ctl_depend() {
	before ppp
	program start $(_br2684ctl)
}

_config_vars="$_config_vars bridge bridge_add brctl"
	
br2684ctl_pre_start() {
	local opts=
	eval opts=\$br2684ctl_${IFVAR}
	[ -z "${opts}" ] && return 0

	if [ "${IFACE#nas[0-9]*}" = "${IFACE}" ]; then
		eerror "Interface must be called nas[0-9] for RFC 2684 Bridging"
		return 1
	fi

	case " ${opts} " in
		*" -b "*|*" -c "*)
			eerror "The -b and -c options are not allowed for br2684ctl_${IVAR}"
			return 1
			;;
		*" -a "*);;
		*)
			eerror "-a option (VPI and VCI) is required in br2684ctl_${IFVAR}"
			return 1
			;;
	esac
	
	einfo "Starting RFC 2684 Bridge control on ${IFACE}"
	start-stop-daemon --start --exec $(_br2684ctl) --background \
		--make-pidfile --pidfile "/var/run/br2684ctl-${IFACE}.pid" \
		-- -c "${IFACE#nas*}" ${opts}
	eend $?
}

br2684ctl_post_stop() {
	local pidfile="/var/run/br2684ctl-${IFACE}.pid"
	[ -e "${pidfile}" ] || return 0
	
	einfo "Stopping RFC 2684 Bridge control on ${IFACE}"
	start-stop-daemon --stop --quiet --pidfile "${pidfile}"
	eend $?
}

# vim: set ts=4 :
