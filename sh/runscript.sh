#!/bin/sh
# Shell wrapper for runscript
# Copyright 1999-2007 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2

. /etc/init.d/functions.sh
. "${RC_LIBDIR}"/sh/rc-functions.sh

# Support LiveCD foo
if [ -r /sbin/livecd-functions.sh ] ; then
	. /sbin/livecd-functions.sh
	livecd_read_commandline
fi

if [ -z "$1" -o -z "$2" ] ; then
	eerror "${SVCNAME}: not enough arguments"
	exit 1
fi

# Descript the init script to the user
describe() {
	if [ -n "${description}" ] ; then
		einfo "${description}"
	else
		ewarn "No description for ${SVCNAME}"
	fi

	local svc= desc=
	for svc in ${opts} ; do
		eval desc=\$description_${svc}
		if [ -n "${desc}" ] ; then
			einfo "${HILITE}${svc}${NORMAL}: ${desc}"
		else
			ewarn "${HILITE}${svc}${NORMAL}: no description"
		fi
	done
}

[ "${RC_DEBUG}" = "yes" ] && set -x

# If we're net.eth0 or openvpn.work then load net or openvpn config
rc_c=${SVCNAME%%.*}
if [ -n "${rc_c}" -a "${rc_c}" != "${SVCNAME}" ] ; then
	if [ -e "/etc/conf.d/${rc_c}.${RC_SOFTLEVEL}" ] ; then
		. "/etc/conf.d/${rc_c}.${RC_SOFTLEVEL}"
	elif [ -e "/etc/conf.d/${rc_c}" ] ; then
		. "/etc/conf.d/${rc_c}"
	fi
fi
unset rc_c

# Overlay with our specific config
if [ -e "/etc/conf.d/${SVCNAME}.${RC_SOFTLEVEL}" ] ; then
	. "/etc/conf.d/${SVCNAME}.${RC_SOFTLEVEL}"
elif [ -e "/etc/conf.d/${SVCNAME}" ] ; then
	. "/etc/conf.d/${SVCNAME}"
fi

# Load any system overrides
[ -e /etc/rc.conf ] && . /etc/rc.conf

# Apply any ulimit defined
[ -n "${RC_ULIMIT}" ] && ulimit ${RC_ULIMIT}

# Load our script
. $1

shift

while [ -n "$1" ] ; do
	# See if we have the required function and run it
	for rc_x in describe start stop ${opts} ; do
		if [ "${rc_x}" = "$1" ] ; then
			if type "$1" >/dev/null 2>/dev/null ; then
				unset rc_x
				"$1" || exit $?
				shift
				continue 2
			else
				if [ "${rc_x}" = "start" -o "${rc_x}" = "stop" ] ; then
					exit 0
				else
					eerror "${SVCNAME}: function \`$1' defined but does not exist"
					exit 1
				fi
			fi	
		fi
	done
	eerror "${SVCNAME}: unknown function \`$1'"
	exit 1
done

# vim: set ts=4 :
