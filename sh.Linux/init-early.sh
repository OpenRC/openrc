#!/bin/sh
# Copyright 1999-2007 Gentoo Foundation
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

# Try and set a font and as early as we can
if [ -e /etc/runlevels/"${RC_DEFAULTLEVEL}"/consolefont \
	 -o -e /etc/runlevels/"${RC_BOOTLEVEL}"/consolefont ] ; then
	termencoding="(K"
	[ -e "${RC_LIBDIR}"/console/unicode ] && termencoding="%G"
	CONSOLE="${CONSOLE:-/dev/console}"
	printf "\033%s" "${termencoding}" >"${CONSOLE}" 2>/dev/null
	if [ -r "${RC_LIBDIR}"/console/font -a -x /bin/setfont ] ; then
		font="$(cat "${RC_LIBDIR}"/console/font)"
		[ -c "${CONSOLE}" ] && cons="-C ${CONSOLE}"
		setfont ${cons} "${RC_LIBDIR}"/console/"${font}" 2>/dev/null
	fi
fi

# Ensure we exit 0 so the boot continues
exit 0
