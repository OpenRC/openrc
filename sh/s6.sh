# Start / stop / status functions for s6 support

# Copyright (c) 2015-2025 The OpenRC Authors.
# See the Authors file at the top-level directory of this distribution and
# https://github.com/OpenRC/openrc/blob/HEAD/AUTHORS
#
# This file is part of OpenRC. It is subject to the license terms in
# the LICENSE file found in the top-level directory of this
# distribution and at https://github.com/OpenRC/openrc/blob/HEAD/LICENSE
# This file may not be copied, modified, propagated, or distributed
#    except according to the terms contained in the LICENSE file.

# Variables that the supervisor=s6 backend understands:
# command, command_args, command_args_foreground (command will be interpreted by sh)
# output_logger, error_logger, output_log, error_log (the former two are mutually exclusive)
# input_file, directory, chroot, umask, command_user
# notify (only fd), stopsig
#
# Extra variables:
# Timeouts: all in milliseconds, 0 for infinity, undefined for no waiting
# timeout_ready: wait for the service to become up, or ready if notify=fd:X
# timeout_down: wait for the service to stop
# timeout_kill: if not stopped after that amount, send a SIGKILL

extra_commands="restart reload ${extra_commands}"

# Call this on every entry point, for readability
_s6_set_variables() {
	name="${name:-${RC_SVCNAME}}"
	_execlineb="$(command -v execlineb)"
	_scandir="${RC_SVCDIR}/s6-scan"
	_servicedirs="${RC_SVCDIR}/s6-services"
	_service="$_scandir/$name"
}

_s6_available() {
	s6-svscanctl -- "$_scandir" 2>/dev/null
}

_execline_available() {
	test -n "$_execlineb" && "$_execlineb" -Pc 'exit 0' 2>/dev/null
}

_s6_sanity_checks() {
	if ! _s6_available ; then
		eerror "No supervision tree running on $_scandir"
		return 1
	fi
	if ! _execline_available ; then
		eerror "No execlineb interpreter accessible"
		return 1
	fi
	if test -n "$notify" && test "${notify##fd:}" = "$notify" ; then
		ewarn "Only the notify=fd method is supported - notify setting will be ignored"
		notify=
	fi
	if test -n "$output_logger" && test -n "$error_logger" ; then
		ewarn "error_logger will be used in priority over output_logger"
		output_logger=
	fi
	if test -n "$output_logger" && test -n "$output_log" ; then
		ewarn "output_logger will be used in priority over output_log"
		output_log=
	fi
	if test -n "$error_logger" && test -n "$error_log" ; then
		ewarn "error_logger will be used in priority over error_log"
		error_log=
	fi
	if test "${output_logger}${error_logger}" = auto ; then
		s6_log_arguments="${s6_log_arguments:-t s1048576 n10}"
	elif test -n "$s6_log_arguments" ; then
		ewarn "s6_log_arguments ignored because the logger was not 'auto'"
		s6_log_arguments=
	fi
	return 0
}

_s6_force_stop() {
	s6-svunlink -- "$_scandir" "$name"
}

_s6_have_legacy_servicedir() {
	test -z "$command" && test -x "/var/svc.d/$name/run"
}

_s6_servicedir_creation_needed() {
	local dir="$_servicedirs/$name" conffile="{$RC_SERVICE%/*}/../conf.d/${RC_SERVICE##*/}"
	if ! test -e "$dir" ; then
		return 0
	fi
	if ! test -d "$dir" ; then
		rm -f -- "$dir"
		return 0
	fi
	if test "$RC_SERVICE" -nt "$dir" || test -f "$conffile" -a "$conffile" -nt "$dir" ; then
		_s6_force_stop
		rm -rf -- "$dir"
		return 0
	fi
	return 1
}

_s6_servicedir_create() {
	local logger="${output_logger}${error_logger}" dir="$_servicedirs/$name"

	mkdir -p -m 0700 -- "$dir"
	if test -n "$notify" && test "${notify##fd:}" != "$notify" ; then
		echo "${notify##fd:}" > "$dir/notification-fd"
	fi
	if test -n "$timeout_kill" ; then
		echo "$timeout_kill" > "$dir/timeout-kill"
	fi
	if test -n "$stopsig" ; then
		echo "$stopsig" > "$dir/down-signal"
	fi

	{
		# We use execline here because it makes code generation easier.
		# The command will still be interpreted by sh in the end.
		echo "#!$_execlineb -S1"
		if test -n "$umask" ; then
			echo "umask \"$umask\""
		fi
		if test -n "$error_logger" ; then
			echo 'fdmove -c 2 1'
		elif test -n "$error_log" ; then
			echo "redirfd -w 2 -- \"$error_log\""
		fi
		if test -n "$output_log" ; then
			echo "redirfd -w 1 -- \"$output_log\""
		fi
		if test -n "$input_file" ; then
			echo "redirfd -r 0 -- \"$input_file\""
		fi
		if test -n "$chroot" ; then
			echo "cd \"$chroot\" chroot ."
		fi
		if test -n "$directory" ; then
			echo "cd \"$directory\""
		fi
		if test -n "$command_user" ; then
			echo "s6-setuidgid \"$command_user\""
		fi
		echo "sh -c \"exec $command $command_args $command_args_foreground\""
	} > "$dir/run"
	chmod 0755 "$dir/run"

	if test -n "$logger" ; then  # TODO: get an unprivileged uid to run the logger as
		mkdir -m 0755 "$dir/log"
		mkdir -p -m 02755 "/var/log/$name"
		if test "$logger" = auto ; then
			echo 3 > "$dir/log/notification-fd"
			{
				echo "#!$_execlineb -S1"
				echo "s6-log -d3 -- ${s6_log_arguments} /var/log/$name"
			} > "$dir/log/run"
		else
			{
				echo '#!/bin/sh -e'
				echo "exec $logger"
			} > "$dir/log/run"
		fi
		chmod 0755 "$dir/log/run"
	fi
	chmod 0755 "$dir"
}

s6_start()
{
	local servicepath r waitcommand waitname
	_s6_set_variables
	if ! _s6_sanity_checks ; then
		eerror "s6 sanity checks failed, cannot start service"
 		return 1
	fi
	servicepath="$_servicedirs/$name"
	if _s6_have_legacy_servicedir ; then
		servicepath="/var/svc.d/$name"
		ebegin "Starting $name (via user-provided service directory)"
	elif _s6_servicedir_creation_needed ; then
		ebegin "Starting $name"
		_s6_servicedir_create
	else
		ebegin "Starting $name (using cached service directory)"
	fi
	if s6-svlink -- "$_scandir" "$servicepath" ; then : ; else
		r=$?
		eend $r "Failed to s6-svlink $servicepath into $_scandir"
		return $r
	fi
	if test -n "$timeout_ready" ; then
		if test -n "$notify" ; then
			waitcommand='-U'
			waitname=ready
		else
			waitcommand='-u'
			waitname=up
		fi
		if s6-svwait $waitcommand -t "$timeout_ready" -- "$_service" ; then : ; else
			r=$?
			s6-svc -d -- "$_service"
			eend $r "Failed to become $waitname in $timeout_ready milliseconds"
			return $r
		fi
	fi
	eend 0
}

s6_stop() {
	_s6_set_variables
	ebegin "Stopping $name"
	s6-svunlink ${timeout_down:+-t} $timeout_down -- "$_scandir" "$name"
	eend $? "Failed to stop $name"
}

s6_status() {
	_s6_set_variables
	if s6-svok "$_service" 2>/dev/null ; then
		s6-svstat -- "$_service"
	else
		_status
	fi
}

restart() {
	_s6_set_variables
	s6-svc -r -- "$_service"
}

reload() {
	_s6_set_variables
	s6-svc -h -- "$_service"
}
