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

# Handy function to handle all our unmounting needs
# mountinfo is a C program to actually find our mounts on our supported OS's
# We rely on fuser being preset, so if it's not then we don't unmount anything.
# This isn't a real issue for the BSD's, but it is for Linux.
do_unmount() {
	type fuser >/dev/null 2>&1 || return 0

	local cmd="$1" retval=0 retry=
	local f_opts="-m -c" f_kill="-s " mnt=
	if [ "${RC_UNAME}" = "Linux" ]; then
		f_opts="-m"
		f_kill="-"
	fi

	shift
	mountinfo "$@" | while read mnt; do
		case "${cmd}" in
			umount*)
				# If we're using the mount (probably /usr) then don't unmount us
				local pids="$(fuser ${f_opts} "${mnt}" 2>/dev/null)"
				case " ${pids} " in
					*" $$ "*)
						ewarn "We are using ${mnt}, not unmounting"
						continue
						;;
				esac
				ebegin "Unmounting ${mnt}"
				;;
			*)
				ebegin "Remounting ${mnt}"
				;;
		esac

		retry=3
		while ! LC_ALL=C ${cmd} "${mnt}" 2>/dev/null; do
			# Don't kill if it's us (/ and possibly /usr)
			local pids="$(fuser ${f_opts} "${mnt}" 2>/dev/null)"
			case " ${pids} " in
				*" $$ "*) retry=0;;
				"  ") eend 1 "in use but fuser finds nothing"; retry=0;;
				*)
					local sig="KILL"
					[ ${retry} -gt 0 ] && sig="TERM"
					fuser ${f_kill}${sig} -k ${f_opts} "${mnt}" \
						>/dev/null 2>&1
					sleep 1
					retry=$((${retry} - 1))
					;;
			esac

			# OK, try forcing things
			if [ ${retry} -le 0 ]; then
				case "${cmd}" in
					umount*)
						LC_ALL=C ${cmd} -f "${mnt}" || retry=-999
						;;
					*)
						retry=-999
						;;
				esac
				break
			fi
		done
		if [ ${retry} -eq -999 ]; then
			eend 1
			retval=1
		else
			eend 0
		fi
	done
	return ${retval}
}

# vim: set ts=4 :
