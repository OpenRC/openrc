#!/bin/sh
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

# If we have a service specific script, run this now
[ -x "${SVCNAME}"-down.sh ] && "${SVCNAME}"-down.sh

# Restore resolv.conf to how it was
if type resolvconf >/dev/null 2>&1; then
	resolvconf -d "${dev}"
elif [ -e /etc/resolv.conf-"${dev}".sv ]; then
	# Important that we copy instead of move incase resolv.conf is
	# a symlink and not an actual file
	cp -p /etc/resolv.conf-"${dev}".sv /etc/resolv.conf
	rm -f /etc/resolv.conf-"${dev}".sv
fi

# Re-enter the init script to stop any dependant services
service=/etc/init.d/"${SVCNAME}"
[ ! -x "${service}" ] && service=/usr/local/etc/init.d/"${SVCNAME}"
if [ -x "${service}" ]; then
	if "${service}" --quiet status; then
		export IN_BACKGROUND=YES
		"${service}" --quiet stop
	fi
fi

exit 0
