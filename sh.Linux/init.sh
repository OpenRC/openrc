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

# udev needs this still
try() {
	"$@"
}

single_user() {
	if [ "${RC_SYS}" = "VPS" ]; then
		einfo "Halting"
		halt -f
		return
	fi

	sulogin ${CONSOLE}
	einfo "Unmounting filesystems"
	if [ -c /dev/null ]; then
		mount -a -o remount,ro 2>/dev/null
	else
		mount -a -o remount,ro
	fi
	einfo "Rebooting"
	reboot -f
}

# This basically mounts $svcdir as a ramdisk, but preserving its content
# which allows us to run depscan.sh
# The tricky part is finding something our kernel supports
# tmpfs and ramfs are easy, so force one or the other
mount_svcdir() {
	local fs= fsopts="-o rw,noexec,nodev,nosuid" devdir="none" devtmp="none" x=
	local svcsize=${svcsize:-1024}

	if grep -Eq "[[:space:]]+tmpfs$" /proc/filesystems; then
		fs="tmpfs"
		fsopts="${fsopts},mode=0755,size=${svcsize}k"
	elif grep -Eq "[[:space:]]+ramfs$" /proc/filesystems; then
		fs="ramfs"
		# ramfs has no special options
	elif [ -e /dev/ram0 -a -e /dev/ram1 ] \
		&& grep -Eq "[[:space:]]+ext2$" /proc/filesystems; then
		devdir="/dev/ram0"
		devtmp="/dev/ram1"
		fs="ext2"
		for x in ${devdir} ${devtmp}; do
			dd if=/dev/zero of="${x}" bs=1k count="${rc_svcsize:-1024}"
			mkfs -t "${fs}" -i 1024 -vm0 "${x}" "${rc_svcsize:-1024}"
		done
	else
		echo
		eerror "OpenRC requires tmpfs, ramfs or 2 ramdisks + ext2"
		eerror "compiled into the kernel"
		echo
		single_user
	fi

	local dotmp=false
	if [ -e "${RC_SVCDIR}"/deptree ]; then
		dotmp=true
		mount -n -t "${fs}" -o rw "${devtmp}" "${RC_LIBDIR}"/tmp
		cp -p "${RC_SVCDIR}"/deptree "${RC_SVCDIR}"/depconfig \
			"${RC_SVCDIR}"/nettree "${RC_LIBDIR}"/tmp 2>/dev/null
	fi

	# If we have no entry in fstab for $svcdir, provide our own
	if fstabinfo --quiet "${RC_SVCDIR}"; then
		mount -n "${RC_SVCDIR}"
	else
		mount -n -t "${fs}" ${fsopts} "${devdir}" "${RC_SVCDIR}"
	fi

	if ${dotmp}; then
		cp -p "${RC_LIBDIR}"/tmp/deptree "${RC_LIBDIR}"/tmp/depconfig \
			"${RC_LIBDIR}"/tmp/nettree "${RC_SVCDIR}" 2>/dev/null
		umount -n "${RC_LIBDIR}"/tmp
	fi
}

_rc_get_kv_cache=
get_KV() {
	[ -z "${_rc_get_kv_cache}" ] \
		&& _rc_get_kv_cache="$(uname -r)"

	echo "$(KV_to_int "${_rc_get_kv_cache}")"

	return $?
}

KV_to_int() {
	[ -z $1 ] && return 1

	local x=${1%%-*}
	local KV_MAJOR=${x%%.*}
	x=${x#*.}
	local KV_MINOR=${x%%.*}
	x=${x#*.}
	local KV_MICRO=${x%%.*}
	local KV_int=$((${KV_MAJOR} * 65536 + ${KV_MINOR} * 256 + ${KV_MICRO} ))

	# We make version 2.2.0 the minimum version we will handle as
	# a sanity check ... if its less, we fail ...
	[ "${KV_int}" -lt 131584 ] && return 1
	
	echo "${KV_int}"
}

. /etc/init.d/functions.sh
. "${RC_LIBDIR}"/sh/rc-functions.sh
[ -r /etc/conf.d/rc ] && . /etc/conf.d/rc
[ -r /etc/rc.conf ] && . /etc/rc.conf

# Compat shim for udev
rc_coldplug=${rc_coldplug:-${RC_COLDPLUG:-yes}}
RC_COLDPLUG=${rc_coldplug}

# Set the console loglevel to 1 for a cleaner boot
# the logger should anyhow dump the ring-0 buffer at start to the
# logs, and that with dmesg can be used to check for problems
if [ -n "${dmesg_level}" -a "${RC_SYS}" != "VPS" ]; then
	dmesg -n "${dmesg_level}"
fi

# By default VServer already has /proc mounted, but OpenVZ does not!
# However, some of our users have an old proc image in /proc
# NFC how they managed that, but the end result means we have to test if
# /proc actually works or not. We to this by comparing uptime to one a second
# ago
mountproc=true
if [ -e /proc/uptime ]; then
	up="$(cat /proc/uptime)"
	sleep 1
	if [ "${up}" = "$(cat /proc/uptime)" ]; then
		eerror "You have cruft in /proc that should be deleted"
	else
		einfo "/proc is already mounted, skipping"
		mountproc=false
	fi
	unset up
fi

if ${mountproc}; then
	procfs="proc"
	[ "${RC_UNAME}" = "GNU/kFreeBSD" ] && proc="linprocfs"
	ebegin "Mounting ${procfs} at /proc"
	if fstabinfo --quiet /proc; then
		mount -n /proc
	else
		mount -n -t "${procfs}" -o noexec,nosuid,nodev proc /proc
	fi
	eend $?
fi
unset mountproc

# Read off the kernel commandline to see if there's any special settings
# especially check to see if we need to set the  CDBOOT environment variable
# Note: /proc MUST be mounted
if [ -r /sbin/livecd-functions.sh ]; then
	. /sbin/livecd-functions.sh
	livecd_read_commandline
fi

[ "$(KV_to_int "$(uname -r)")" -ge "$(KV_to_int "2.6.0")" ]
K26=$?

if [ "${RC_UNAME}" != "GNU/kFreeBSD" -a "${RC_SYS}" != "VPS" -a "${K26}" = "0" ]; then
	if [ -d /sys ]; then
		if ! mountinfo --quiet /sys; then
			ebegin "Mounting sysfs at /sys"
			if fstabinfo --quiet /sys; then
				mount -n /sys
			else
				mount -n -t sysfs -o noexec,nosuid,nodev sysfs /sys
			fi
			eend $?
		fi
	else
		ewarn "No /sys to mount sysfs needed in 2.6 and later kernels!"
	fi
fi

devfs_mounted=
if [ -e /dev/.devfsd ]; then
	# make sure devfs is actually mounted and it isnt a bogus file
	devfs_mounted=$(mountinfo --fstype-regex devfs)
fi

# Try to figure out how the user wants /dev handled
#  - check $RC_DEVICES from /etc/conf.d/rc
#  - check boot parameters
#  - make sure the required binaries exist
#  - make sure the kernel has support
if [ "${rc_devices}" = "static" -o "${RC_SYS}" = "VPS" ]; then
	ebegin "Using existing device nodes in /dev"
	eend 0
elif [ "${RC_UNAME}" = "GNU/kFreeBSD" ]; then
	ebegin "Using kFreeBSD devfs in /dev"
	eend 0
else
	case ${rc_devices} in
		devfs)  managers="devfs udev mdev";;
		udev)   managers="udev devfs mdev";;
		mdev)   managers="mdev udev devfs";;
		auto|*) managers="udev devfs mdev";;
	esac

	for m in ${managers}; do
		# Check common manager prerequisites and kernel params
		if get_bootparam "no${m}" || ! has_addon ${m}-start; then
			continue
		fi
		# Check specific manager prerequisites
		case ${m} in
			udev|mdev)
				if [ -n "${devfs_mounted}" -o "${K26}" != 0 ]; then
					continue
				fi
				;;
			devfs)
				grep -Eqs "[[:space:]]+devfs$" /proc/filesystems || continue
				;;
		esac

		# Let's see if we can get this puppy rolling
		start_addon ${m} && break
	done
fi

# Mount required stuff as user may not have then in /etc/fstab
for x in "devpts /dev/pts 0755 ,gid=5,mode=0620" "tmpfs /dev/shm 1777 ,nodev"; do
	set -- ${x}
	grep -Eq "[[:space:]]+$1$" /proc/filesystems || continue
	mountinfo -q "$2" && continue

	if [ ! -d "$2" ] && \
	   [ "${m}" = "devfs" -o "${m}" = "udev" ]; then
		mkdir -m "$3" -p "$2" >/dev/null 2>&1 || \
			ewarn "Could not create $2!"
	fi

	if [ -d "$2" ]; then
		ebegin "Mounting $1 at $2"
		if fstabinfo --quiet "$2"; then
			mount -n "$2"
		else
			mount -n -t "$1" -o noexec,nosuid"$4" none "$2"
		fi
		eend $?
	fi
done

# If booting off CD, we want to update inittab before setting the runlevel
if [ -f /sbin/livecd-functions.sh -a -n "${CDBOOT}" ]; then
	ebegin "Updating inittab"
	livecd_fix_inittab
	eend $?
	telinit q &>/dev/null
fi

. "${RC_LIBDIR}"/sh/init-common-post.sh

# vim: set ts=4 :
