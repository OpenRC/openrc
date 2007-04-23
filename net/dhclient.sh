# Copyright 2004-2007 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2

dhclient_depend() {
	after interface
	program start /sbin/dhclient
	provide dhcp
}

_config_vars="$_config_vars dhcp dhcpcd"

dhclient_start() {
	local args= opt= opts= pidfile="/var/run/dhclient-${IFACE}.pid"
	local sendhost=true dconf=

	# Get our options
	eval opts=\$dhcp_${IFVAR}
	[ -z "${opts}" ] && opts=${dhcp}

	# Map some generic options to dhcpcd
	for opt in ${opts} ; do
		case "${opt}" in
			nodns) args="${args} -e PEER_DNS=no" ;;
			nontp) args="${args} -e PEER_NTP=no" ;;
			nogateway) args="${args} -e PEER_ROUTERS=no" ;;
			nosendhost) sendhost=false ;;
		esac
	done

	# Add our route metric
	[ "${metric:-0}" != "0" ] && args="${args} -e IF_METRIC=${metric}"

	if ${sendhost} ; then
		local hname="$(hostname)"
		if [ "${hname}" != "(none)" -a "${hname}" != "localhost" ]; then
			dhconf="${dhconf} interface \"${IFACE}\" {"
			dhconf="${dhconf} send host-name \"${hname}\";"
			dhconf="${dhconf}}"
		fi
	fi

	# Bring up DHCP for this interface
	ebegin "Running dhclient"
	set -x
	echo "${dhconf}" | start-stop-daemon --start --exec /sbin/dhclient \
		--pidfile "${pidfile}" -- ${args} -q -1 -pf "${pidfile}" "${IFACE}"
	eend $? || return 1
	set +x

	_show_address
	return 0
}

dhclient_stop() {
	local pidfile="/var/run/dhclient-${IFACE}.pid" opts=
	[ ! -f "${pidfile}" ] && return 0

	# Get our options
	if [ -x /sbin/dhclient ] ; then
		eval opts=\$dhcp_${IFVAR}
		[ -z "${opts}" ] && opts=${dhcp}
	fi

	ebegin "Stopping dhclient on ${IFACE}"
	case " ${opts} " in
		*" release "*) dhclient -q -r -pf "${pidfile}" "${IFACE}" ;;
		*)
			start-stop-daemon --stop --quiet \
				--exec /sbin/dhclient --pidfile "${pidfile}"
			;;
	esac
	eend $?
}

# vim: set ts=4 :
