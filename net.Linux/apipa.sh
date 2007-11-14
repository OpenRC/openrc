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

apipa_depend() {
	program /sbin/arping
}

_random() {
	if [ -n "${BASH}" ] ; then
		echo "${RANDOM}"
	else
		uuidgen | sed -n -e 's/[^[:digit:]]//g' -e 's/\(^.\{1,7\}\).*/\1/p'
	fi
}

apipa_start() {
	local iface="$1" i1= i2= addr= i=0

	_exists true || return 1
	
	einfo "Searching for free addresses in 169.254.0.0/16"
	eindent

	while [ ${i} -lt 64516 ]; do
		i1=$((($(_random) % 255) + 1))
		i2=$((($(_random) % 255) + 1))

		addr="169.254.${i1}.${i2}"
		vebegin "${addr}/16"
		if ! arping_address "${addr}" ; then
			eval config_${config_index}="\"${addr}/16 broadcast 169.254.255.255\""
			config_index=$((${config_index} - 1))
			veend 0
			eoutdent
			return 0
		fi

		i=$((${i} + 1))
	done

	eerror "No free address found!"
	eoutdent
	return 1
}

# vim: set ts=4 :
