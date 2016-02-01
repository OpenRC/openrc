# start / stop / status functions for supervise-daemon

# Copyright (c) 2016 The OpenRC Authors.
# See the Authors file at the top-level directory of this distribution and
# https://github.com/OpenRC/openrc/blob/master/AUTHORS
#
# This file is part of OpenRC. It is subject to the license terms in
# the LICENSE file found in the top-level directory of this
# distribution and at https://github.com/OpenRC/openrc/blob/master/LICENSE
# This file may not be copied, modified, propagated, or distributed
#    except according to the terms contained in the LICENSE file.

supervise_start()
{
	if [ -z "$command" ]; then
		ewarn "The command variable is undefined."
		ewarn "There is nothing for ${name:-$RC_SVCNAME} to start."
		return 1
	fi

	ebegin "Starting ${name:-$RC_SVCNAME}"
	eval supervise-daemon --start \
		${pidfile:+--pidfile} $pidfile \
		${command_user+--user} $command_user \
		$supervise_daemon_args \
		$command \
		-- $command_args $command_args_foreground
	rc=$?
	[ -n "${pidfile}" ] && service_set_value "pidfile" "${pidfile}"
	eend $rc "failed to start $RC_SVCNAME"
}

supervise_stop()
{
	local startpidfile="$(service_get_value "pidfile")"
	pidfile="${startpidfile:-$pidfile}"
	[ -n "$pidfile" ] || return 0
	ebegin "Stopping ${name:-$RC_SVCNAME}"
	supervise-daemon --stop \
		${pidfile:+--pidfile} $pidfile \
		${stopsig:+--signal} $stopsig

	eend $? "Failed to stop $RC_SVCNAME"
}

supervise_status()
{
	_status
}
