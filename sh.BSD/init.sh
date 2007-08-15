#!/bin/sh
# Copyright 1999-2007 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2

# void single_user()
#
#  Drop to a shell, remount / ro, and then reboot
#
single_user() {
	exit 1
}

# This basically mounts $svcdir as a ramdisk, but preserving its content
# which allows us to run depscan.sh
# FreeBSD has a nice ramdisk - we don't set a size as we should always
# be fairly small and we unmount them after the boot level is done anyway
# NOTE we don't set a size for Linux either
mount_svcdir() {
	local dotmp=false
	if [ -e "${RC_SVCDIR}"/deptree ] ; then
		dotmp=true
		try mdconfig -a -t malloc -s 1m -u 1
		try newfs /dev/md1
		try mount /dev/md1 "${RC_LIBDIR}"/tmp
		cp -p "${RC_SVCDIR}"/deptree "${RC_SVCDIR}"/depconfig \
			"${RC_SVCDIR}"/nettree "${RC_LIBDIR}"/tmp 2>/dev/null
	fi
	try mdconfig -a -t malloc -s "${RC_SVCSIZE:-1024}"k -u 0
	try newfs -b 4096 -i 1024 -n /dev/md0
	try mount -o rw,noexec,nosuid /dev/md0 "${RC_SVCDIR}"
	if ${dotmp} ; then
		cp -p "${RC_LIBDIR}"/tmp/deptree "${RC_LIBDIR}"/tmp/depconfig \
			"${RC_LIBDIR}"/tmp/nettree "${RC_SVCDIR}" 2>/dev/null
		try umount "${RC_LIBDIR}"/tmp
		try mdconfig -d -u 1
	fi
}

. "${RC_LIBDIR}"/sh/init-functions.sh
. "${RC_LIBDIR}"/sh/functions.sh

# Disable devd until we need it
[ "${RC_UNAME}" = "FreeBSD" ] && sysctl hw.bus.devctl_disable=1 >/dev/null

. "${RC_LIBDIR}"/sh/init-common-post.sh

# vim: set ts=4 :
