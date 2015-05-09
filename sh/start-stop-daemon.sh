# start / stop / status functions for start-stop-daemon
# Copyright (c) 2007-2009 Roy Marples <roy@marples.name>
# Released under the 2-clause BSD license.

ssd_start()
{
	[ -n "$command" ] || return 0

	local _background=
	ebegin "Starting ${name:-$RC_SVCNAME}"
	if yesno "${command_background}"; then
		if [ -z "${pidfile}" ]; then
			eend 1 "command_background option used but no pidfile specified"
			return 1
		fi
		if [ -n "${command_args_background}" ]; then
			eend 1 "command_background used with command_args_background"
			return 1
		fi
		_background="--background --make-pidfile"
	fi
	if yesno "$start_inactive"; then
		local _inactive=false
		service_inactive && _inactive=true
		mark_service_inactive
	fi
	eval start-stop-daemon --start \
		--exec $command \
		${procname:+--name} $procname \
		${pidfile:+--pidfile} $pidfile \
		${command_user+--user} $command_user \
		$_background $start_stop_daemon_args \
		-- $command_args $command_args_background
	if eend $? "Failed to start $RC_SVCNAME"; then
		service_set_value "command" "${command}"
		[ -n "${pidfile}" ] && service_set_value "pidfile" "${pidfile}"
		[ -n "${procname}" ] && service_set_value "procname" "${procname}"
		return 0
	fi
	if yesno "$start_inactive"; then
		if ! $_inactive; then
			mark_service_stopped
		fi
	fi
	return 1
}

ssd_stop()
{
	local startcommand="$(service_get_value "command")"
	local startpidfile="$(service_get_value "pidfile")"
	local startprocname="$(service_get_value "procname")"
	command="${startcommand:-$command}"
	pidfile="${startpidfile:-$pidfile}"
	procname="${startprocname:-$procname}"
	[ -n "$command" -o -n "$procname" -o -n "$pidfile" ] || return 0
	ebegin "Stopping ${name:-$RC_SVCNAME}"
	start-stop-daemon --stop \
		${retry:+--retry} $retry \
		${command:+--exec} $command \
		${procname:+--name} $procname \
		${pidfile:+--pidfile} $pidfile \
		${stopsig:+--signal} $stopsig

	eend $? "Failed to stop $RC_SVCNAME"
}

ssd_status()
{
	_status
}
