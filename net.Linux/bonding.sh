# Copyright 2004-2007 Gentoo Foundation
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

bonding_depend() {
	before interface macchanger
	program /sbin/ifenslave
}

_config_vars="$_config_vars slaves"

_is_bond() {
	[ -f "/proc/net/bonding/${IFACE}" ]
}

bonding_pre_start() {
	local s= slaves="$(_get_array "slaves_${IFVAR}")" 

	[ -z "${slaves}" ] && return 0

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
	einfo "${slaves}"

	# Check that our slaves exist
	(
	for IFACE in ${slaves}; do
		_exists true || return 1
	done

	# Must force the slaves to a particular state before adding them
	for IFACE in ${slaves}; do
		_delete_addresses
		_up
	done
	)

	# now force the master to up
	_up

	# finally add in slaves
	eoutdent
	/sbin/ifenslave "${IFACE}" ${slaves} >/dev/null
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
