# Start / stop / status functions for s6 support
# Copyright (c) 2015 William Hubbs <w.d.hubbs@gmail.com>
# Released under the 2-clause BSD license.

[ -z "${s6_service_path}" ] && s6_service_path="/var/svc.d/${RC_SVCNAME}"

s6_start()
{
	if [ ! -d "${s6_service_path}" ]; then
		eerror "${s6_service_path} does not exist."
 	return 1
 fi
	s6_service_link="${RC_SVCDIR}/s6-scan/${s6_service_path##*/}"
	ebegin "Starting ${name:-$RC_SVCNAME}"
	ln -sf "${s6_service_path}" "${s6_service_link}"
	s6-svscanctl -na "${RC_SVCDIR}"/s6-scan
	sleep 1.5
	s6-svc -u "${s6_service_link}"
	if [ -n "$s6_svwait_options_start" ]; then
		s6-svwait ${s6_svwait_options_start} "${s6_service_link}"
	fi
	sleep 1.5
	set -- $(s6-svstat "${s6_service_link}")
	[ "$1" = "up" ]
	eend $? "Failed to start $RC_SVCNAME"
}

s6_stop()
{
	if [ ! -d "${s6_service_path}" ]; then
		eerror "${s6_service_path} does not exist."
 	return 1
 fi
	s6_service_link="${RC_SVCDIR}/s6-scan/${s6_service_path##*/}"
	ebegin "Stopping ${name:-$RC_SVCNAME}"
	s6-svc -Dd -T ${s6_service_timeout_stop:-10000} "${s6_service_link}"
	set -- $(s6-svstat "${s6_service_link}")
	[ "$1" = "down" ]
	eend $? "Failed to stop $RC_SVCNAME"
}

s6_status()
{
	s6_service_link="${RC_SVCDIR}/s6-scan/${s6_service_path##*/}"
	if [ -L "${s6_service_link}" ]; then
		s6-svstat "${s6_service_link}"
	else
		_status
	fi
}
