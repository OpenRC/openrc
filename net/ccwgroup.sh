# Copyright (c) 2007-2008 Roy Marples <roy@marples.name>
# All rights reserved. Released under the 2-clause BSD license.

_config_vars="$_config_vars ccwgroup"

ccwgroup_depend()
{
	before interface
}

ccwgroup_load_modules()
{
	# make sure we have ccwgroup support or this is a crap shoot
	if [ ! -d /sys/bus/ccwgroup ] ; then
		modprobe -q ccwgroup
		if [ ! -d /sys/bus/ccwgroup ] ; then
			eerror "ccwgroup support missing in kernel"
			return 1
		fi
	fi

	# verify the specific interface is supported
	if [ ! -d /sys/bus/ccwgroup/drivers/$1 ] ; then
		modprobe $1 >& /dev/null
		if [ ! -d /sys/bus/ccwgroup/drivers/$1 ] ; then
			eerror "$1 support missing in kernel"
			return 1
		fi
	fi

	return 0
}

ccwgroup_pre_start()
{
	local ccwgroup="$(_get_array "ccwgroup_${IFVAR}")"
	[ -z "${ccwgroup}" ] && return 0

	local ccw_type
	eval ccw_type=\${ccwgroup_type_${IFVAR}:-qeth}

	ccwgroup_load_modules ${ccw_type} || return 1

	einfo "Enabling ccwgroup/${ccw_type} on ${IFACE}"

	set -- ${ccwgroup}
	local first=$1; shift
	if [ -e /sys/devices/${ccw_type}/${first}/online ]; then
		echo "0" >/sys/devices/${ccw_type}/${first}/online
	else
		echo "${first}$(printf ',%s' "$@")" >/sys/bus/ccwgroup/drivers/${ccw_type}/group
	fi

	local var val
	for var in $(_get_array "ccwgroup_opts_${IFVAR}") online=1 ; do
		val=${var#*=}
		var=${var%%=*}
		echo "${val}" > /sys/devices/${ccw_type}/${first}/${var}
	done
	eend $?

	# Now that we've properly configured the device, we can run
	# bring the interface up.  Common code tried to do this already,
	# but it failed because we didn't setup sysfs yet.
	_up
}

ccwgroup_pre_stop()
{
	local path="/sys/class/net/${IFACE}"

	# Erase any existing ccwgroup to be safe
	service_set_value ccwgroup_device ""
	service_set_value ccwgroup_type ""

	[ ! -L "${path}"/device/driver ] && return 0
	case "$(readlink "${path}"/device/driver)" in
		*/bus/ccwgroup/*) ;;
		*) return 0;;
	esac

	local device
	device="$(readlink "${path}"/device)"
	device=${device##*/}
	service_set_value ccwgroup_device "${device}"
	device="$(readlink "${path}"/device/driver)"
	device=${device##*/}
	service_set_value ccwgroup_type "${device}"
}

ccwgroup_post_stop()
{
	local device="$(service_get_value ccwgroup_device)"
	[ -z "${device}" ] && return 0
	local ccw_type="$(service_get_value ccwgroup_type)"
	local path="/sys/devices/${ccw_type}/${device}"

	einfo "Disabling ccwgroup/${ccw_type} on ${IFACE}"
	if echo "0" >"${path}"/online &&
	   echo "1" >"${path}"/ungroup ; then
		# The device doesn't disappear right away which breaks
		# restart, or a quick start up, so wait around.
		while [ -e "${path}" ] ; do :; done
	fi
	eend $?
}
