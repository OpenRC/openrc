#!/bin/sh
# Shell wrapper to list our dependencies
# Copyright 2007 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2

. /etc/init.d/functions.sh

need() {
	exec 1>&3
	[ -n "$*" ] && echo "${SVCNAME} ineed $*"
	exec 1>&2
}
use() {
	exec 1>&3
	[ -n "$*" ] && echo "${SVCNAME} iuse $*"
	exec 1>&2
}
before() {
	exec 1>&3
	[ -n "$*" ] && echo "${SVCNAME} ibefore $*"
	exec 1>&2
}
after() {
	exec 1>&3
	[ -n "$*" ] && echo "${SVCNAME} iafter $*"
	exec 1>&2
}
provide() {
	exec 1>&3
	[ -n "$*" ] && echo "${SVCNAME} iprovide $*"
	exec 1>&2
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
		[ -e /etc/conf.d/"${rc_c}" ] && . /etc/conf.d/"${rc_c}" >&2
	fi
	unset rc_c

	[ -e /etc/conf.d/"${SVCNAME}" ] && . /etc/conf.d/"${SVCNAME}" >&2
	
	if . /etc/init.d/"${SVCNAME}" ; then
		exec 1>&3
		echo "${SVCNAME}"
		exec 1>&2

	    depend

		# Add any user defined depends
		need ${RC_NEED}
		use ${RC_USE}
		before ${RC_BEFORE}
		after ${RC_AFTER}
		provide ${RC_PROVIDE}
    fi
    )
done

# vim: set ts=4 :
