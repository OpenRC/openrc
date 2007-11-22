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
			eerror "To function properly, \"$1\" needs to exist."
			if yesno ${RC_FORCE_AUTO}; then
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
