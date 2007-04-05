# Copyright 2007 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2

has_addon() {
	[ -e "${RC_LIBDIR}/addons/$1.sh" ]
}

import_addon() {
	if has_addon "$1" ; then
		. "${RC_LIBDIR}/addons/$1.sh"
		return 0
	fi
	return 1
}

start_addon() {
	( import_addon "$1-start" )
}

stop_addon() {
	( import_addon "$1-stop" )
}

is_net_fs() {
	[ -z "$1" ] && return 1

	local t=$(mountinfo --fstype "$1" )
	for x in ${RC_NET_FS_LIST} ; do
		[ "${x}" = "${t}" ] && return 0
	done
	return 1
}

is_union_fs() {
	[ ! -x /sbin/unionctl ] && return 1
	unionctl "$1" --list >/dev/null 2>/dev/null
}

get_bootparam() {
	local match="$1"
	[ -z "${match}" -o ! -r /proc/cmdline ] && return 1

	set -- $(cat /proc/cmdline)
	while [ -n "$1" ] ; do
		case "$1" in
			gentoo=*)
				local params="${1##*=}"
				local IFS=, x=
				for x in ${params} ; do
					[ "${x}" = "${match}" ] && return 0
				done
				;;
		esac
		shift
	done

	return 1
}

# vim: set ts=4 :
