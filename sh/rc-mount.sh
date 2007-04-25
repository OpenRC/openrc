# Copyright 2007 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2

# bool do_unmount(char *cmd, char *no_unmounts, char *nodes, char *fslist)
# Handy function to handle all our unmounting needs
# find-mount is a C program to actually find our mounts on our supported OS's
do_unmount() {
	local cmd="$1" retval=0 retry=
	local f_opts="-m -c" f_kill="-s " mnt=
	if [ "${RC_UNAME}" = "Linux" ] ; then
		f_opts="-c"
		f_kill="-"
	fi

	local mnts="$(mountinfo ${2:+--skip-regex} $2 ${3:+--node-regex} $3 ${4:+--fstype-regex} $4 --reverse \
	| sed -e "s/'/'\\\\''/g" -e "s/^/'/g" -e "s/$/'/g")"
	eval set -- ${mnts}
	for mnt in "$@" ; do
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
		while ! ${cmd} "${mnt}" 2>/dev/null ; do
			# Don't kill if it's us (/ and possibly /usr)
			local pids="$(fuser ${f_opts} "${mnt}" 2>/dev/null)"
			case " ${pids} " in
				*" $$ "*) retry=0 ;;
				"  ") eend 1 "in use but fuser finds nothing"; retry=0 ;;
				*)
					local sig="KILL"
					[ ${retry} -gt 0 ] && sig="TERM"
					fuser ${f_kill}${sig} -k ${f_opts} "${mnt}" \
						>/dev/null 2>/dev/null
					sleep 1
					retry=$((${retry} - 1))
					;;
			esac

			# OK, try forcing things
			if [ ${retry} -le 0 ] ; then
				local extra_opts="-f"
				case "${cmd}" in
					mount*)
						# Silly reiserfs helper blocks us, so bypass it
						[ "${RC_UNAME}" = "Linux" ] && extra_opts="-i" 
						;;
				esac
				${cmd} ${extra_opts} "${mnt}" || retry=-999
				break
			fi
		done
		if [ ${retry} -eq -999 ] ; then
			eend 1
			retval=1
		else
			eend 0
		fi
	done
	return ${retval}
}

# vim: set ts=4 :
