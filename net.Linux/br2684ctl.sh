# Copyright 2004-2007 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2

br2684ctl_depend() {
	before ppp
	program start /sbin/br2684ctl
}

_config_vars="$_config_vars bridge bridge_add brctl"
	
br2684ctl_pre_start() {
	local opts=
	eval opts=\$br2684ctl_${IFVAR}
	[ -z "${opts}" ] && return 0

	if [ "${IFACE#nas[0-9]*}" = "${IFACE}" ] ; then
		eerror "Interface must be called nas[0-9] for RFC 2684 Bridging"
		return 1
	fi

	case " ${opts} " in
		*" -b "*|*" -c "*)
			eerror "The -b and -c options are not allowed for br2684ctl_${IVAR}"
			return 1
			;;
		*" -a "*) ;;
		*)
			eerror "-a option (VPI and VCI) is required in br2684ctl_${IFVAR}"
			return 1
			;;
	esac
	
	einfo "Starting RFC 2684 Bridge control on ${IFACE}"
	start-stop-daemon --start --exec /sbin/br2684ctl --background \
		--make-pidfile --pidfile "/var/run/br2684ctl-${IFACE}.pid" \
		-- -c "${IFACE#nas*}" ${opts}
	eend $?
}

br2684ctl_post_stop() {
	local pidfile="/var/run/br2684ctl-${IFACE}.pid"
	[ -e "${pidfile}" ] || return 0
	
	einfo "Stopping RFC 2684 Bridge control on ${IFACE}"
	start-stop-daemon --stop --quiet \
		--exec /sbin/br2684ctl --pidfile "${pidfile}"
	eend $?
}

# vim: set ts=4 :
