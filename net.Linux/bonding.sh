# Copyright 2004-2007 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2

bonding_depend() {
	before interface macchanger
	program /sbin/ifenslave
}

_config_vars="$_config_vars slaves"

_is_bond() {
	[ -f "/proc/net/bonding/${IFACE}" ]
}

bonding_pre_start() {
	local s= slaves= 

	eval $(_get_array "slaves_${IFVAR}")
	[ $# = "0" ] && return 0

	# Load the kernel module if required
	if [ ! -d /proc/net/bonding ] ; then
		if ! modprobe bonding ; then
			eerror "Cannot load the bonding module"
			return 1
		fi
	fi

	# We can create the interface name we like now, but this
	# requires sysfs
	if ! _exists && [ -d /sys/class/net ] ; then
		echo "+${IFACE}" > /sys/class/net/bonding_masters
	fi
	_exists true || return 1

	if ! _is_bond ; then
		eerror "${IFACE} is not capable of bonding"
		return 1
	fi

	ebegin "Adding slaves to ${IFACE}"
	eindent
	einfo "$@"

	# Check that our slaves exist
	(
	for IFACE in "$@" ; do
		_exists true || return 1
	done

	# Must force the slaves to a particular state before adding them
	for IFACE in "$@" ; do
		_delete_addresses
		_up
	done
	)

	# now force the master to up
	_up

	# finally add in slaves
	eoutdent
	/sbin/ifenslave "${IFACE}" $@ >/dev/null
	eend $?

	return 0 #important
}

bonding_stop() {
	_is_bond || return 0	

	local slaves= s=
	slaves=$( \
		sed -n -e 's/^Slave Interface: //p' "/proc/net/bonding/${IFACE}" \
		| tr '\n' ' ' \
	)
	[ -z "${slaves}" ] && return 0

	# remove all slaves
	ebegin "Removing slaves from ${IFACE}"
	eindent
	einfo "${slaves}"
	eoutdent
	/sbin/ifenslave -d "${IFACE}" ${slaves}

	# reset all slaves
	(
	for IFACE in ${slaves}; do
		if _exists ; then
			_delete_addresses
			_down
		fi
	done
	)

	eend 0
	return 0
}

# vim: set ts=4 :
