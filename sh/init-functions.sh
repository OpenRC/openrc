# Copyright 1999-2007 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2

# void try(command)
#
#   Try to execute 'command', if it fails, drop to a shell.
#
try() {
	local errstr
	local retval=0
	
	if [ -c /dev/null ] ; then
		errstr=$(eval $* 2>&1 >/dev/null)
	else
		errstr=$(eval $* 2>&1)
	fi
	retval=$?
	if [ ${retval} -ne 0 ] ; then
		#splash "critical" &
		eend 1
		eerror "The \"$*\" command failed with error:"
		eerror "  ${errstr#*: }"
		echo
		eerror "Since this is a critical task, startup cannot continue."
		echo
		single_user
	fi
	
	return ${retval}
}

# bool check_statedir(dir)
#
#   Check that 'dir' exists, if not, drop to a shell.
#
check_statedir() {
	[ -z "$1" ] && return 0

	if [ ! -d "$1" ] ; then
		if ! mkdir -p "$1" &>/dev/null ; then
			#splash "critical" &
			echo
			eerror "For Gentoo to function properly, \"$1\" needs to exist."
			if [ "${RC_FORCE_AUTO}" = "yes" ] ; then
				eerror "Attempting to create \"$1\" for you ..."
				mount -o remount,rw /
				mkdir -p "$1"
			fi
			if [ ! -d "$1" ] ; then
				eerror "Please mount your root partition read/write, and execute:"
				echo
				eerror "  # mkdir -p $1"
				echo; echo
				single_user
			fi
		fi
	fi

	return 0
}

# vim: set ts=4 :
