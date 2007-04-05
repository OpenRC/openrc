# Copyright 2004-2007 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2

ipppd_depend() {
	program start /usr/sbin/ipppd
	after macnet
	before interface
	provide isdn
}

_config_vars="$_config_vars ipppd"

ipppd_pre_start() {
	local opts= pidfile="/var/run/ipppd-${IFACE}.pid"

	# Check that we are a valid ippp interface
	case "${IFACE}" in
		ippp[0-9]*) ;;
		*) return 0 ;;
	esac

	# Check that the interface exists
	_exists || return 1

	# Might or might not be set in conf.d/net
	eval opts=\$ipppd_${IFVAR}

	einfo "Starting ipppd for ${IFACE}"
	start-stop-daemon --start --exec /usr/sbin/ipppd \
		--pidfile "${pidfile}" \
		-- ${opts} pidfile "${pidfile}" \
		file "/etc/ppp/options.${IFACE}" >/dev/null
	eend $?
}

ipppd_post_stop() {
	local pidfile="/var/run/ipppd-${IFACE}.pid"

	[ ! -f "${pidfile}" ] && return 0

	einfo "Stopping ipppd for ${IFACE}"
	start-stop-daemon --stop --quiet --exec /usr/sbin/ipppd \
		--pidfile "${pidfile}"
	eend $?
}

# vim: set ts=4 :
