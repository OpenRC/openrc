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

bridge_depend() {
	before interface macnet
	program /sbin/brctl
}

_config_vars="$_config_vars bridge bridge_add brctl"

_is_bridge() {
	brctl show 2>/dev/null | grep -q "^${IFACE}[[:space:]]"
}

bridge_pre_start() {
	local ports= brif= iface="${IFACE}" e= x=
	local ports="$(_get_array "bridge_${IFVAR}")"
	local opts="$(_get_array "brctl_${IFVAR}")"
	
	eval brif=\$bridge_add_${IFVAR}
	[ -z "${ports}" -a -z "${brif}" -a -z "${opts}" ] && return 0

	[ -n "${ports}" ] && bridge_post_stop

	(
	if [ -z "${ports}" -a -n "${brif}" ]; then
		ports="${IFACE}"
		IFACE="${brif}"
	else
		ports="${ports}"
		metric=1000
	fi

	if ! _is_bridge; then
		ebegin "Creating bridge ${IFACE}"
		if ! brctl addbr "${IFACE}"; then
			eend 1
			return 1
		fi
	fi

	local IFS="$__IFS"
	for x in ${opts}; do
		unset IFS
		set -- ${x}
		x=$1
		shift
		set -- "${x}" "${IFACE}" "$@"
		brctl "$@"
	done
	unset IFS

	if [ -n "${ports}" ]; then
		einfo "Adding ports to ${IFACE}"
		eindent

		local OIFACE="${IFACE}"
		for x in ${ports}; do
			ebegin "${x}"
			local IFACE="${x}"
			_set_flag promisc
			_up
			if ! brctl addif "${OIFACE}" "${x}"; then
				_set_flag -promisc
				eend 1
				return 1
			fi
			eend 0
		done
		eoutdent
	fi
	) || return 1

	# Bring up the bridge
	_up
}

bridge_post_stop() {
	local port= ports= delete=false extra=

	if _is_bridge; then
		ebegin "Destroying bridge ${IFACE}"
		_down
		ports="$(brctl show 2>/dev/null | \
			sed -n -e '/^'"${IFACE}"'[[:space:]]/,/^\S/ { /^\('"${IFACE}"'[[:space:]]\|\t\)/s/^.*\t//p }')"
		delete=true
		iface=${IFACE}
		eindent
	else
		# Work out if we're added to a bridge for removal or not
		eval set -- $(brctl show 2>/dev/null | sed -e "s/'/'\\\\''/g" -e "s/$/'/g" -e "s/^/'/g")
		local line=
		for line; do
			set -- ${line}
			if [ "$3" = "${IFACE}" ]; then
				iface=$1
				break
			fi
		done
		[ -z "${iface}" ] && return 0
		extra=" from ${iface}"
	fi

	for port in ${ports}; do
		ebegin "Removing port ${port}${extra}"
		local IFACE="${port}"
		_set_flag -promisc
		brctl delif "${iface}" "${port}"
		eend $?
	done

	if ${delete}; then
		eoutdent
		brctl delbr "${iface}"
		eend $?
	fi
	
	return 0
}

# vim: set ts=4 :
