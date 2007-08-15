#!/bin/sh
# Shell wrapper to list our dependencies
# Copyright 2007 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2

. /etc/init.d/functions.sh

config() {
	[ -n "$*" ] && echo "${SVCNAME} config $*" >&3
}
need() {
	[ -n "$*" ] && echo "${SVCNAME} ineed $*" >&3
}
use() {
	[ -n "$*" ] && echo "${SVCNAME} iuse $*" >&3
}
before() {
	[ -n "$*" ] && echo "${SVCNAME} ibefore $*" >&3
}
after() {
	[ -n "$*" ] && echo "${SVCNAME} iafter $*" >&3
}
provide() {
	[ -n "$*" ] && echo "${SVCNAME} iprovide $*" >&3
} 
depend() {
	:
}

cd /etc/init.d
for SVCNAME in * ; do
	[ -x "${SVCNAME}" ] || continue
	case "${SVCNAME}" in
		*.sh) continue ;;
	esac

	SVCNAME=${SVCNAME##*/}
	(
	# Save stdout in fd3, then remap it to stderr
	exec 3>&1 1>&2

	rc_c=${SVCNAME%%.*}
	if [ -n "${rc_c}" -a "${rc_c}" != "${SVCNAME}" ] ; then
		[ -e /etc/conf.d/"${rc_c}" ] && . /etc/conf.d/"${rc_c}"
	fi
	unset rc_c

	[ -e /etc/conf.d/"${SVCNAME}" ] && . /etc/conf.d/"${SVCNAME}"

	if . /etc/init.d/"${SVCNAME}" ; then
		echo "${SVCNAME}" >&3
		depend

		# Add any user defined depends
		config ${RC_CONFIG}
		need ${RC_NEED}
		use ${RC_USE}
		before ${RC_BEFORE}
		after ${RC_AFTER}
		provide ${RC_PROVIDE}
	fi
	)
done

# vim: set ts=4 :
