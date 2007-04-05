# Copyright 2004-2007 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2

bridge_depend() {
	before interface macnet
	program /sbin/brctl
}

_config_vars="$_config_vars bridge bridge_add brctl"

_is_bridge() {
	brctl show 2>/dev/null | grep -q "^${IFACE}[[:space:]]"
}

bridge_pre_start() {
	local ports= brif= opts= iface="${IFACE}" e= x=
	eval $(_get_array "bridge_${IFVAR}")
	ports="$@"
	eval brif=\$bridge_add_${IFVAR}
	eval $(_get_array "brctl_${IFVAR}")
	opts="$@"
	[ -z "${ports}" -a -z "${brif}" -a -z "${opts}" ] && return 0

	[ -n "${ports}" ] && bridge_post_stop

	(
	if [ -z "${ports}" -a -n "${brif}" ] ; then
		ports="${IFACE}"
		IFACE="${brif}"
	else
		ports="${ports}"
		metric=1000
	fi

	if ! _is_bridge ; then
		ebegin "Creating bridge ${IFACE}"
		if ! brctl addbr "${IFACE}" ; then
			eend 1
			return 1
		fi

		eval set -- ${opts}
		for x in "$@" ; do
			case " ${x} " in
				*" ${IFACE} "*) ;;
				*) x="${x} ${IFACE}" ;;
			esac
			brctl ${x}
		done
	fi

	if [ -n "${ports}" ] ; then
		einfo "Adding ports to ${IFACE}"
		eindent

		eval set -- ${ports}
		for x in "$@" ; do
			ebegin "${x}"
			if ! ifconfig "${x}" promisc up && brctl addif "${IFACE}" "${x}" ; then
				ifconfig "${x}" -promisc 2>/dev/null
				eend 1
				return 1
			fi
			eend 0
		done
		eoutdent
	fi
	)
}

bridge_post_stop() {
	local port= ports= delete=false extra=

	if _is_bridge ; then
		ebegin "Destroying bridge ${IFACE}"
		_down
		ports="$( brctl show 2>/dev/null | \
			sed -n -e '/^'"${IFACE}"'[[:space:]]/,/^\S/ { /^\('"${IFACE}"'[[:space:]]\|\t\)/s/^.*\t//p }')"
		delete=true
		iface=${IFACE}
		eindent
	else
		# Work out if we're added to a bridge for removal or not
		eval set -- $(brctl show 2>/dev/null | sed -e "s/'/'\\\\''/g" -e "s/$/'/g" -e "s/^/'/g")
		local line=
		for line in "$@" ; do
			set -- ${line}
			if [ "$3" = "${IFACE}" ] ; then
				iface=$1
				break
			fi
		done
		[ -z "${iface}" ] && return 0
		extra=" from ${iface}"
	fi

	for port in ${ports} ; do
		ebegin "Removing port ${port}${extra}"
		ifconfig "${port}" -promisc
		brctl delif "${iface}" "${port}"
		eend $?
	done

	if ${delete} ; then
		eoutdent
		brctl delbr "${iface}"
		eend $?
	fi
	
	return 0
}

# vim: set ts=4 :
