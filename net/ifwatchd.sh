# Copyright (c) 2007-2008 Roy Marples <roy@marples.name>
# All rights reserved. Released under the 2-clause BSD license.

_config_vars="$_config_vars plug_timeout"

ifwatchd_depend()
{
	program start /usr/sbin/ifwatchd
	after macnet rename wireless
	before interface
	provide plug
}

ifwatchd_pre_start()
{
	# We don't start ifwatchd if we're being called from the background
	yesno ${IN_BACKGROUND} && return 0

	_exists || return 0

	# We need a valid MAC address
	# It's a basic test to ensure it's not a virtual interface
	if ! _get_mac_address >/dev/null 2>&1; then
		vewarn "ifwatchd only works on interfaces with a valid MAC address"
		return 0
	fi

	ebegin "Starting ifwatchd on ${IFACE}"

	# Mark the us as inactive so ifwatchd can restart us
	mark_service_inactive

	# Start ifwatchd
	export IN_BACKGROUND=yes
	start-stop-daemon --start --exec /usr/sbin/ifwatchd \
		-- -c "${RC_LIBDIR}/sh/ifwatchd-carrier.sh" \
		-n "${RC_LIBDIR}/sh/ifwatchd-nocarrier.sh" "${IFACE}"
	unset IN_BACKGROUND
	eend "$?" || return 1

	einfo "Backgrounding ..."
	exit 1
}

ifwatchd_stop()
{
	yesno ${IN_BACKGROUND} && return 0

	start-stop-daemon --test --quiet --stop --exec /usr/sbin/ifwatchd \
		-- -c "${RC_LIBDIR}/sh/ifwatchd-carrier.sh" \
		-n "${RC_LIBDIR}/sh/ifwatchd-nocarrier.sh" "${IFACE}" \
		|| return 0
	
	ebegin "Stopping ifwatchd on" "${IFACE}"
	start-stop-daemon --stop --exec /usr/sbin/ifwatchd \
		-- -c "${RC_LIBDIR}/sh/ifwatchd-carrier.sh" \
		-n "${RC_LIBDIR}/sh/ifwatchd-nocarrier.sh" "${IFACE}"
	eend $?
}
