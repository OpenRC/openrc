#!/bin/sh
# Copyright 1999-2007 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2

# void single_user()
#
#  Drop to a shell, remount / ro, and then reboot
#
single_user() {
	if [ "${RC_SYS}" = "VPS" ] ; then
		einfo "Halting"
		halt -f
		return
	fi

	sulogin ${CONSOLE}
	einfo "Unmounting filesystems"
	if [ -c /dev/null ] ; then
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
	local mntcmd=$(fstabinfo --mountcmd "${RC_LIBDIR}")

	if grep -Eq "[[:space:]]+tmpfs$" /proc/filesystems ; then
		fs="tmpfs"
		fsopts="${fsopts},mode=0755,size=${svcsize}k"
	elif grep -Eq "[[:space:]]+ramfs$" /proc/filesystems ; then
		fs="ramfs"
		# ramfs has no special options
	elif [ -e /dev/ram0 -a -e /dev/ram1 ] \
		&& grep -Eq "[[:space:]]+ext2$" /proc/filesystems ; then
		devdir="/dev/ram0"
		devtmp="/dev/ram1"
		fs="ext2"
		for x in ${devdir} ${devtmp} ; do
			try dd if=/dev/zero of="${x}" bs=1k count="${svcsize}"
			try mkfs -t "${fs}" -i 1024 -vm0 "${x}" "${svcsize}"
		done
	else
		echo
		eerror "Gentoo Linux requires tmpfs, ramfs or 2 ramdisks + ext2"
		eerror "compiled into the kernel"
		echo
		single_user
	fi

	# If we have no entry in fstab for $svcdir, provide our own
	if [ -z "${mntcmd}" ] ; then
		mntcmd="-t ${fs} ${fsopts} ${devdir} ${RC_SVCDIR}"
	fi

	local dotmp=false
	if [ -e "${RC_SVCDIR}"/deptree ] ; then
		dotmp=true
		try mount -n -t "${fs}" -o rw "${devtmp}" "${RC_LIBDIR}"/tmp
		cp -p "${RC_SVCDIR}"/deptree "${RC_SVCDIR}"/nettree \
			"${RC_LIBDIR}"/tmp 2>/dev/null
	fi
	try mount -n ${mntcmd}
	if ${dotmp} ; then
		cp -p "${RC_LIBDIR}"/tmp/deptree "${RC_LIBDIR}"/tmp/nettree \
			"${RC_SVCDIR}" 2>/dev/null
		try umount -n "${RC_LIBDIR}"/tmp
	fi
}

_RC_GET_KV_CACHE=""
get_KV() {
	[ -z "${_RC_GET_KV_CACHE}" ] \
		&& _RC_GET_KV_CACHE="$(uname -r)"

	echo "$(KV_to_int "${_RC_GET_KV_CACHE}")"

	return $?
}

# Try and set a font as early as we can
ttydev=${CONSOLE:-/dev/tty1}
if [ ! -c "${ttydev}" ] ; then
	[ -c /dev/vc/1 ] && ttydev="/dev/vc/1" || ttydev=
fi
[ -r "${RC_LIBDIR}"/console/font ] \
	&& /bin/setfont ${ttydev:+-C} ${ttydev} "${RC_LIBDIR}"/console/font
[ -r "${RC_LIBDIR}"/console/map ] \
	&& /bin/setfont ${ttydev:+-C} ${ttydev} -m "${RC_LIBDIR}"/console/map
[ -r "${RC_LIBDIR}"/console/unimap ] \
	&& /bin/setfont ${ttydev:+-C} ${ttydev} -u "${RC_LIBDIR}"/console/unimap
if [ -e "${RC_LIBDIR}"/console/unicode ] ; then
	eval printf "\033%%G" ${ttydev:+>} ${ttydev}
else
	eval printf "\033(K" ${ttydev:+>} ${ttydev}
fi
unset ttydev

. /etc/init.d/functions.sh
. "${RC_LIBDIR}"/sh/init-functions.sh
. "${RC_LIBDIR}"/sh/rc-functions.sh

# Set the console loglevel to 1 for a cleaner boot
# the logger should anyhow dump the ring-0 buffer at start to the
# logs, and that with dmesg can be used to check for problems
${RC_DMESG_LEVEL+/bin/dmesg -n ${RC_DMESG_LEVEL}}

check_statedir /proc

# By default VServer already has /proc mounted, but OpenVZ does not!
if [ ! -e /proc/self/stat ] ; then
	procfs="proc"
	[ "${RC_UNAME}" = "GNU/kFreeBSD" ] && proc="linprocfs"
	ebegin "Mounting ${procfs} at /proc"
	mntcmd="$(fstabinfo --mountcmd /proc)"
	try mount -n ${mntcmd:--t ${procfs} -o noexec,nosuid,nodev proc /proc}
	eend $?
fi

# Read off the kernel commandline to see if there's any special settings
# especially check to see if we need to set the  CDBOOT environment variable
# Note: /proc MUST be mounted
if [ -r /sbin/livecd-functions.sh ] ; then
	. /sbin/livecd-functions.sh
	livecd_read_commandline
fi

[ "$(KV_to_int "$(uname -r)")" -ge "$(KV_to_int "2.6.0")" ]
K26=$?

if [ "${RC_UNAME}" != "GNU/kFreeBSD" -a "${RC_NAME}" != "VPS" -a "${K26}" = "0" ] ; then
	if [ -d /sys ] ; then
		ebegin "Mounting sysfs at /sys"
		mntcmd="$(fstabinfo --mountcmd /sys)"
		try mount -n ${mntcmd:--t sysfs -o noexec,nosuid,nodev sysfs /sys}
		eend $?
	else
		ewarn "No /sys to mount sysfs needed in 2.6 and later kernels!"
	fi
fi

check_statedir /dev

devfs_mounted=
if [ -e /dev/.devfsd ] ; then
	# make sure devfs is actually mounted and it isnt a bogus file
	devfs_mounted=$(mountinfo --fstype-regex devfs)
fi

# Try to figure out how the user wants /dev handled
#  - check $RC_DEVICES from /etc/conf.d/rc
#  - check boot parameters
#  - make sure the required binaries exist
#  - make sure the kernel has support
if [ "${RC_DEVICES}" = "static" -o "${RC_SYS}" = "VPS" ] ; then
	ebegin "Using existing device nodes in /dev"
	eend 0
elif [ "${RC_UNAME}" = "GNU/kFreeBSD" ] ; then
	ebegin "Using kFreeBSD devfs in /dev"
	eend 0
else
	case ${RC_DEVICES} in
		devfs)  managers="devfs udev mdev";;
		udev)   managers="udev devfs mdev";;
		mdev)   managers="mdev udev devfs";;
		auto|*) managers="udev devfs mdev";;
	esac

	for m in ${managers} ; do
		# Check common manager prerequisites and kernel params
		if get_bootparam "no${m}" || ! has_addon ${m}-start ; then
			continue
		fi
		# Check specific manager prerequisites
		case ${m} in
			udev|mdev)
				if [ -n "${devfs_mounted}" -o "${K26}" != 0 ] ; then
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

# From linux-2.6 we need to mount /dev/pts again ...
if [ "${RC_UNAME}" != "GNU/kFreeBSD" -a "${K26}" = "0" ] ; then
	if grep -Eq "[[:space:]]+devpts$" /proc/filesystems && \
		! mountinfo /dev/pts > /dev/null ; then
		if [ ! -d /dev/pts ] && \
		   [ "${devfs}" = "yes" -o "${udev}" = "yes" ] ; then
			# Make sure we have /dev/pts
			mkdir -p /dev/pts >/dev/null 2>/dev/null || \
				ewarn "Could not create /dev/pts!"
		fi

		if [ -d /dev/pts ] ; then
			ebegin "Mounting devpts at /dev/pts"
			mntcmd="$(fstabinfo --mountcmd /dev/pts)"
			try mount -n ${mntcmd:--t devpts -o gid=5,mode=0620,noexec,nosuid devpts /dev/pts}
			eend $?
		fi
	fi
fi

# If booting off CD, we want to update inittab before setting the runlevel
if [ -f /sbin/livecd-functions.sh -a -n "${CDBOOT}" ] ; then
	ebegin "Updating inittab"
	livecd_fix_inittab
	eend $?
	/sbin/telinit q &>/dev/null
fi

. "${RC_LIBDIR}"/sh/init-common-post.sh

# vim: set ts=4 :
