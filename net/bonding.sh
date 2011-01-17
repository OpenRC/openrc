# Copyright (c) 2007-2008 Roy Marples <roy@marples.name>
# All rights reserved. Released under the 2-clause BSD license.

bonding_depend()
{
	before interface macchanger
}

_config_vars="$_config_vars slaves"

_is_bond()
{
	[ -f "/proc/net/bonding/${IFACE}" ]
}

bonding_pre_start()
{
	local x= s= n= slaves= primary=

	slaves="$(_get_array "slaves_${IFVAR}")"
	unset slaves_${IFVAR}

	eval primary="\$primary_${IFVAR}"
	unset primary_${IFVAR}


	[ -z "${slaves}" ] && return 0

	# Load the kernel module if required
	if [ ! -d /proc/net/bonding ]; then
		if ! modprobe bonding; then
			eerror "Cannot load the bonding module"
			return 1
		fi
	fi

	# We can create the interface name we like now, but this
	# requires sysfs
	if ! _exists && [ -d /sys/class/net ]; then
		echo "+${IFACE}" > /sys/class/net/bonding_masters
	fi
	_exists true || return 1

	if ! _is_bond; then
		eerror "${IFACE} is not capable of bonding"
		return 1
	fi

	# Interface must be down in order to configure
	_down

	# Configure the bond mode, then we can reloop to ensure we configure
	# All other options
	for x in /sys/class/net/"${IFACE}"/bonding/mode; do
		[ -f "${x}" ] || continue
		n=${x##*/}
		eval s=\$${n}_${IFVAR}
		if [ -n "${s}" ]; then
			einfo "Setting ${n}: ${s}"
			echo "${s}" >"${x}" || \
			eerror "Failed to configure $n (${n}_${IFVAR})"
		fi
	done
	# Nice and dynamic for remaining options:)
	for x in /sys/class/net/"${IFACE}"/bonding/*; do
		[ -f "${x}" ] || continue
		n=${x##*/}
		eval s=\$${n}_${IFVAR}
		[ "${n}" != "mode" ] || continue
		if [ -n "${s}" ]; then
			einfo "Setting ${n}: ${s}"
			echo "${s}" >"${x}" || \
			eerror "Failed to configure $n (${n}_${IFVAR})"
		fi
	done

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
		_down
	done
	)

	# now force the master to up
	_up

	# finally add in slaves
	eoutdent
	if [ -d /sys/class/net ]; then
		if [ -n "${primary}" ]; then
			echo "+${primary}" >/sys/class/net/"${IFACE}"/bonding/slaves
			echo "${primary}" >/sys/class/net/"${IFACE}"/bonding/primary
			slaves="${slaves/${primary}/}"
		fi
		for s in ${slaves}; do
			echo "+${s}" >/sys/class/net/"${IFACE}"/bonding/slaves
		done
	else
		/sbin/ifenslave "${IFACE}" ${slaves} >/dev/null
	fi
	eend $?

	return 0 #important
}

bonding_stop()
{
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
	if [ -d /sys/class/net ]; then
		for s in ${slaves}; do
			echo -"${s}" > /sys/class/net/"${IFACE}"/bonding/slaves
		done
	else
		/sbin/ifenslave -d "${IFACE}" ${slaves}
	fi

	# reset all slaves
	(
	for IFACE in ${slaves}; do
		if _exists; then
			_delete_addresses
			_down
		fi
	done
	)

	_down

	if [ -d /sys/class/net ]; then
		echo "-${IFACE}" > /sys/class/net/bonding_masters
	fi

	eend 0
	return 0
}
