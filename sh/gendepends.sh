#!/bin/sh
# Shell wrapper to list our dependencies
# Copyright 2007 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2

. /etc/init.d/functions.sh

need() {
    [ -n "$*" ] && echo "${SVCNAME} ineed $*"
}
use() {
    [ -n "$*" ] && echo "${SVCNAME} iuse $*"
}
before() {
    [ -n "$*" ] && echo "${SVCNAME} ibefore $*"
}
after() {
    [ -n "$*" ] && echo "${SVCNAME} iafter $*"
}
provide() {
    [ -n "$*" ] && echo "${SVCNAME} iprovide $*"
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
	if . /etc/init.d/"${SVCNAME}" ; then
	    rc_c=${SVCNAME%%.*}
	    if [ -n "${rc_c}" -a "${rc_c}" != "${SVCNAME}" ] ; then
			[ -e /etc/conf.d/"${rc_c}" ] && . /etc/conf.d/"${rc_c}"
	    fi
	    unset rc_c

		[ -e /etc/conf.d/"${SVCNAME}" ] && . /etc/conf.d/"${SVCNAME}"

		echo "${SVCNAME}"
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
