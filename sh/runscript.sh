#!/bin/sh
# Shell wrapper for runscript

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

# Support LiveCD foo
if [ -r /sbin/livecd-functions.sh ]; then
	. /sbin/livecd-functions.sh
	livecd_read_commandline
fi

if [ -z "$1" -o -z "$2" ]; then
	eerror "${SVCNAME}: not enough arguments"
	exit 1
fi

# Descript the init script to the user
describe() {
	if [ -n "${description}" ]; then
		einfo "${description}"
	else
		ewarn "No description for ${SVCNAME}"
	fi

	local svc= desc=
	for svc in ${extra_commands:-${opts}}; do
		eval desc=\$description_${svc}
		if [ -n "${desc}" ]; then
			einfo "${HILITE}${svc}${NORMAL}: ${desc}"
		else
			ewarn "${HILITE}${svc}${NORMAL}: no description"
		fi
	done
}

yesno ${RC_DEBUG} && set -x

# If we're net.eth0 or openvpn.work then load net or openvpn config
_c=${SVCNAME%%.*}
if [ -n "${_c}" -a "${_c}" != "${SVCNAME}" ]; then
	if [ -e "/etc/conf.d/${_c}.${RC_SOFTLEVEL}" ]; then
		. "/etc/conf.d/${_c}.${RC_SOFTLEVEL}"
	elif [ -e "/etc/conf.d/${_c}" ]; then
		. "/etc/conf.d/${_c}"
	fi
fi
unset _c

# Overlay with our specific config
if [ -e "/etc/conf.d/${SVCNAME}.${RC_SOFTLEVEL}" ]; then
	. "/etc/conf.d/${SVCNAME}.${RC_SOFTLEVEL}"
elif [ -e "/etc/conf.d/${SVCNAME}" ]; then
	. "/etc/conf.d/${SVCNAME}"
fi

# Load any system overrides
[ -e /etc/rc.conf ] && . /etc/rc.conf

# Apply any ulimit defined
[ -n "${rc_ulimit:-${RC_ULIMIT}}" ] && ulimit ${rc_ulimit:-${RC_ULIMIT}}

# Load our script
. $1
shift

for _d in ${required_dirs}; do
	if [ ! -d ${_d} ]; then
		eerror "${SVCNAME}: \`${_d}' is not a directory"
		exit 1
	fi
done
unset _d

for _f in ${required_files}; do
	if [ ! -r ${_f} ]; then
		eerror "${SVCNAME}: \`${_f}' is not readable"
		exit 1
	fi
done
unset _f

# If we have a default command then supply a default start function
if [ -n "${command}" ]; then
	if ! type start >/dev/null 2>&1; then
		start() {
			ebegin "Starting ${name:-${SVCNAME}}"
			case "${command_background}" in
				[Yy][Ee][Ss]|[Tt][Rr][Uu][Ee]|[Oo][Nn]|1)
					start_stop_daemon_args="${start_stop_daemon_args} --background --pidfile"
					;;
			esac
			yesno "${start_inactive}" && mark_service_inactive "${SVCNAME}"
			start-stop-daemon --start \
				--exec ${command} \
				${procname:+--name} ${procname} \
				${pidfile:+--pidfile} ${pidfile} \
				${start_stop_daemon_args} \
				-- ${command_args}
			eend $? "Failed to start ${SVCNAME}"
		}
	fi
fi

# If we have a default command, procname or pidfile then supply a default stop 
# function
if [ -n "${command}" -o -n "${procname}" -o -n "${pidfile}" ]; then
	if ! type stop >/dev/null 2>&1; then
		stop() {
			ebegin "Stopping ${name:-${SVCNAME}}"
			start-stop-daemon --stop \
				${command:+--exec} ${command} \
				${procname:+--name} ${procname} \
				${pidfile:+--pidfile} ${pidfile}
			eend $? "Failed to stop ${SVCNAME}"
		}
	fi
fi

while [ -n "$1" ]; do
	# See if we have the required function and run it
	for _cmd in describe start stop ${extra_commands:-${opts}} \
		${extra_started_commands}; do
		if [ "${_cmd}" = "$1" ]; then
			if type "$1" >/dev/null 2>&1; then
				# If we're in the background, we may wish to fake some
				# commands. We do this so we can "start" ourselves from
				# inactive which then triggers other services to start
				# which depend on us. A good example of this is openvpn.
				if yesno ${IN_BACKGROUND}; then
					for _cmd in ${in_background_fake}; do
						if [ "${_cmd}" = "$1" ]; then
							shift
							continue 3
						fi
					done
				fi
				# Check to see if we need to be started before we can run
				# this command
				for _cmd in ${extra_started_commands}; do
					if [ "${_cmd}" = "$1" ]; then
						if ! service_started "${SVCNAME}"; then
							eerror "${SVCNAME}: cannot \`$1' as it has not been started"
							exit 1
						fi
					fi
				done
				unset _cmd
				if type "$1"_pre >/dev/null 2>&1; then
					"$1"_pre || exit $?
				fi
				"$1" || exit $?
				if type "$1"_post >/dev/null 2>&1; then
					"$1"_post || exit $?
				fi
				shift
				continue 2
			else
				if [ "${_cmd}" = "start" -o "${_cmd}" = "stop" ]; then
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
