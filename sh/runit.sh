# Copyright (c) 2016 The OpenRC Authors.
# See the Authors file at the top-level directory of this distribution and
# https://github.com/OpenRC/openrc/blob/HEAD/AUTHORS
#
# This file is part of OpenRC. It is subject to the license terms in
# the LICENSE file found in the top-level directory of this
# distribution and at https://github.com/OpenRC/openrc/blob/HEAD/LICENSE
# This file may not be copied, modified, propagated, or distributed
#    except according to the terms contained in the LICENSE file.
# Released under the 2-clause BSD license.

runit_start()
{
	local service_path service_link
	service_path="${runit_service:-/etc/sv/${RC_SVCNAME}}"
	if [ ! -d "${service_path}" ]; then
		eerror "Runit service ${service_path} not found"
		return 1
	fi
	service_link="${RC_SVCDIR}/sv/${service_path##*/}"
	ebegin "Starting ${name:-$RC_SVCNAME}"
	ln -snf "${service_path}" "${service_link}"
	local i=0 retval=1
	# it can take upto 5 seconds for runsv to start
	while [ $i -lt 6 ] ; do
		if sv start "${service_link}" > /dev/null 2>&1; then
			retval=0
			break
		fi
		sleep 1 && i=$(expr $i + 1)
	done
	if [ $retval -eq 1 ]; then
		# clean up the link else sv will keep on trying
		rm "${service_link}"
	fi
	eend $retval "Failed to start ${name:-$RC_SVCNAME}"
}

runit_stop()
{
	local service_path service_link
	service_path="${runit_service:-/etc/sv/${RC_SVCNAME}}"
	if [ ! -d "${service_path}" ]; then
		eerror "Runit service ${service_path} not found"
		return 1
	fi
	service_link="${RC_SVCDIR}/sv/${service_path##*/}"
	ebegin "Stopping ${name:-$RC_SVCNAME}"
	sv stop "${service_link}" > /dev/null 2>&1 &&
	rm "${service_link}"
	eend $? "Failed to stop ${name:-$RC_SVCNAME}"
}

runit_status()
{
	local service_path service_link
	service_path="${runit_service:-/etc/sv/${RC_SVCNAME}}"
	if [ ! -d "${service_path}" ]; then
		eerror "Runit service ${service_path} not found"
		return 1
	fi
	service_link="${RC_SVCDIR}/sv/${service_path##*/}"
	sv status "${service_link}"
}
