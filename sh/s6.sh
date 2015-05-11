# Start / stop / status functions for s6 support
# Copyright (c) 2015 William Hubbs <w.d.hubbs@gmail.com>
# Released under the 2-clause BSD license.

[ -z "${s6_service_path}" ] && s6_service_path="/etc/svc.d/${RC_SVCNAME}"

s6_start()
{
	if [ ! -d "${s6_service_path}" ]; then
		eerror "${s6_service_path} does not exist."
 	return 1
 fi
	local rc
	ebegin "Starting ${name:-$RC_SVCNAME}"
	ln -sf "${s6_service_path}" "${RC_SVCDIR}"/s6-scan
	s6-svscanctl -an "${RC_SVCDIR}"/s6-scan
	rc=$?
	if [ -n "$s6_svwait_options_start" ]; then
		s6-svwait ${s6_svwait_options_start} "${s6_service_path}"
		rc=$?
	fi
	eend $rc "Failed to start $RC_SVCNAME"
}

s6_stop()
{
	if [ ! -d "${s6_service_path}" ]; then
		eerror "${s6_service_path} does not exist."
 	return 1
 fi
	local rc
	ebegin "Stopping ${name:-$RC_SVCNAME}"
	rm -rf "${RC_SVCDIR}/s6-scan/${s6_service_path##*/}"
	s6-svscanctl -an "${RC_SVCDIR}"/s6-scan
	rc=$? 
	if [ -n "$s6_svwait_options_stop" ]; then
		s6-svwait ${s6_svwait_options_stop} "${s6_service_path}"
		rc=$?
	fi
	eend $rc "Failed to stop $RC_SVCNAME"
}

s6_status()
{
	s6-svstat "${s6_service_path}"
}
