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

. /etc/init.d/functions.sh
. "${RC_LIBDIR}"/sh/rc-functions.sh
[ -r /etc/rc.conf ] && . /etc/rc.conf

# Support LiveCD foo
if [ -r /sbin/livecd-functions.sh ]; then
	. /sbin/livecd-functions.sh
	livecd_read_commandline
fi

stop_addon devfs
stop_addon udev

# Really kill things off before unmounting
if [ -x /sbin/killall5 ]; then
	killall5 -15
	killall5 -9
fi

# Flush all pending disk writes now
sync; sync

# If we are in a VPS, we don't need anything below here, because
#   1) we don't need (and by default can't) umount anything (VServer) or
#   2) the host utils take care of all umounting stuff (OpenVZ)
if [ "${RC_SYS}" = "VPS" ]; then
	if [ -e /etc/init.d/"$1".sh ]; then
		. /etc/init.d/"$1".sh
	else
		exit 0
	fi
fi

# If $svcdir is still mounted, preserve it if we can

mnt=$(mountinfo --node "${RC_SVCDIR}")
if [ -n "${mnt}" -a -w "${RC_LIBDIR}" ]; then
	f_opts="-m -c"
	[ "${RC_UNAME}" = "Linux" ] && f_opts="-c"
	if [ -n "$(fuser ${f_opts} "${svcdir}" 2>/dev/null)" ]; then
		fuser -k ${f_opts} "${svcdir}" >/dev/null 2>&1
		sleep 2
	fi
	cp -p "${RC_SVCDIR}"/deptree "${RC_SVCDIR}"/depconfig \
		"${RC_SVCDIR}"/softlevel "${RC_SVCDIR}"/nettree \
		"${RC_SVCDIR}"/rc.log \
		"${RC_LIBDIR}" 2>/dev/null
	umount "${RC_SVCDIR}"
	rm -rf "${RC_SVCDIR}"/*
	# Pipe errors to /dev/null as we may have future timestamps
	cp -p "${RC_LIBDIR}"/deptree "${RC_LIBDIR}"/depconfig \
		"${RC_LIBDIR}"/softlevel "${RC_LIBDIR}"/nettree \
		"${RC_LIBDIR}"/rc.log \
		"${RC_SVCDIR}" 2>/dev/null
	rm -f "${RC_LIBDIR}"/deptree "${RC_LIBDIR}"/depconfig \
		"${RC_LIBDIR}"/softlevel "${RC_LIBDIR}"/nettree \
		"${RC_LIBDIR}"/rc.log
	# Release the memory disk if we used it
	case "${mnt}" in
		"/dev/md"[0-9]*) mdconfig -d -u "${mnt#/dev/md*}";;
	esac
fi

unmounted=0
# Remount the remaining filesystems read-only
# Most BSD's don't need this as the kernel handles it nicely
if [ "${RC_UNAME}" = "Linux" ]; then
	ebegin "Remounting remaining filesystems read-only"
	# We need the do_unmount function
	. "${RC_LIBDIR}"/sh/rc-mount.sh
	eindent
	fs=
	for x in ${net_fs_list}; do
		fs="${fs}${fs:+|}${x}"
	done
	[ -n "${fs}" ] && fs="^(${fs})$"
	do_unmount "mount -n -o remount,ro" \
		--skip-point-regex "^(/dev|/dev/.*|/proc|/proc/.*|/sys|/sys/.*)$" \
		${fs:+--skip-fstype-regex} ${fs} --nonetdev
	eoutdent
	eend $?
	unmounted=$?
fi

# This UPS code should be moved to out of here and to an addon
if [ -f /etc/killpower ]; then
	UPS_CTL=/sbin/upsdrvctl
	UPS_POWERDOWN="${UPS_CTL} shutdown"
elif [ -f /etc/apcupsd/powerfail ]; then
	UPS_CTL=/etc/apcupsd/apccontrol
	UPS_POWERDOWN="${UPS_CTL} killpower"
fi
if [ -x "${UPS_CTL}" ]; then
	ewarn "Signalling ups driver(s) to kill the load!"
	${UPS_POWERDOWN}
	ewarn "Halt system and wait for the UPS to kill our power"
	halt -id
	sleep 60
fi

if [ ${unmounted} -ne 0 ]; then
	[ -x /sbin/sulogin ] && sulogin -t 10 /dev/console
	exit 1
fi

# Load the final script - not needed on BSD so they should not exist
[ -e /etc/init.d/"$1".sh ] && . /etc/init.d/"$1".sh

# Always exit 0 here
exit 0

# vim: set ts=4 :
