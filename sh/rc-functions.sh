# Copyright 2007 Gentoo Foundation
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

has_addon() {
	[ -e "${RC_LIBDIR}/addons/$1.sh" ] || [ -e /lib/rcscripts/addons/"$1".sh ]
}

import_addon() {
	if [ -e "${RC_LIBDIR}/addons/$1.sh" ]; then
		. "${RC_LIBDIR}/addons/$1.sh"
	elif [ -e /lib/rcscripts/addons/"$1".sh ]; then
		. /lib/rcscripts/addons/"$1".sh
	else
		return 1
	fi
}

start_addon() {
	( import_addon "$1-start" )
}

stop_addon() {
	( import_addon "$1-stop" )
}

net_fs_list="afs cifs coda davfs fuse gfs ncpfs nfs nfs4 ocfs2 shfs smbfs"
is_net_fs() {
	[ -z "$1" ] && return 1

	# Check OS specific flags to see if we're local or net mounted
	mountinfo --quiet --netdev "$1"  && return 0
	mountinfo --quiet --nonetdev "$1" && return 1

	# Fall back on fs types
	local t=$(mountinfo --fstype "$1")
	for x in ${net_fs_list}; do
		[ "${x}" = "${t}" ] && return 0
	done
	return 1
}

is_union_fs() {
	[ ! -x /sbin/unionctl ] && return 1
	unionctl "$1" --list >/dev/null 2>&1
}

get_bootparam() {
	local match="$1"
	[ -z "${match}" -o ! -r /proc/cmdline ] && return 1

	set -- $(cat /proc/cmdline)
	while [ -n "$1" ]; do
		case "$1" in
			gentoo=*)
				local params="${1##*=}"
				local IFS=, x=
				for x in ${params}; do
					[ "${x}" = "${match}" ] && return 0
				done
				;;
		esac
		shift
	done

	return 1
}

# Add our sbin to $PATH
case "${PATH}" in
	/lib/rc/sbin|/lib/rc/sbin:*);;
	*) export PATH="/lib/rc/sbin:${PATH}";;
esac

# vim: set ts=4 :
