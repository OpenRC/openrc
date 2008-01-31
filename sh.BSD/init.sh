#!/bin/sh
# Copyright 2007-2008 Roy Marples <roy@marples.name>
# All rights reserved. Released under the 2-clause BSD license.

# This basically mounts $svcdir as a ramdisk, but preserving its content
# which allows us to run depscan.sh
# FreeBSD has a nice ramdisk - we don't set a size as we should always
# be fairly small and we unmount them after the boot level is done anyway
# NOTE we don't set a size for Linux either
# FreeBSD-7 supports tmpfs now :)
mount_svcdir()
{
	local dotmp=false release=false retval=0
	if [ -e "${RC_SVCDIR}"/deptree ]; then
		dotmp=true
		if ! mount -t tmpfs none "${RC_LIBDIR}"/tmp 2>/dev/null; then
			mdconfig -a -t malloc -s 1m -u 1
			newfs /dev/md1
			mount /dev/md1 "${RC_LIBDIR}"/tmp
			release=true
		fi
		cp -p "${RC_SVCDIR}"/deptree "${RC_SVCDIR}"/depconfig \
			"${RC_SVCDIR}"/nettree "${RC_LIBDIR}"/tmp 2>/dev/null
	fi
	if ! mount -t tmpfs -o rw,noexec,nosuid none "${RC_SVCDIR}" 2>/dev/null; then
		mdconfig -a -t malloc -s "${rc_svcsize:-1024}"k -u 0
		newfs -b 4096 -i 1024 -n /dev/md0
		mount -o rw,noexec,nosuid /dev/md0 "${RC_SVCDIR}"
	fi
	retval=$?
	if ${dotmp}; then
		cp -p "${RC_LIBDIR}"/tmp/deptree "${RC_LIBDIR}"/tmp/depconfig \
			"${RC_LIBDIR}"/tmp/nettree "${RC_SVCDIR}" 2>/dev/null
		umount "${RC_LIBDIR}"/tmp
		${release} && mdconfig -d -u 1
	fi

	return ${retval}
}

. "${RC_LIBDIR}"/sh/functions.sh
[ -r /etc/rc.conf ] && . /etc/rc.conf

# Disable devd until we need it
[ "${RC_UNAME}" = "FreeBSD" ] && sysctl hw.bus.devctl_disable=1 >/dev/null

. "${RC_LIBDIR}"/sh/init-common-post.sh
