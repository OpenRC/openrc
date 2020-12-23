# Copyright (c) 2007 Gentoo Foundation
# Copyright (c) 2007-2009 Roy Marples <roy@marples.name>
# Released under the 2-clause BSD license.

net_fs_list="afs ceph cifs coda davfs fuse fuse.sshfs gfs glusterfs lustre
ncpfs nfs nfs4 ocfs2 shfs smbfs"
is_net_fs()
{
	[ -z "$1" ] && return 1

	# Check OS specific flags to see if we're local or net mounted
	mountinfo --quiet --netdev "$1"  && return 0
	mountinfo --quiet --nonetdev "$1" && return 1

	# Fall back on fs types
	local t=$(mountinfo --fstype "$1")
	for x in $net_fs_list $extra_net_fs_list; do
		[ "$x" = "$t" ] && return 0
	done
	return 1
}

is_union_fs()
{
	[ ! -x /sbin/unionctl ] && return 1
	unionctl "$1" --list >/dev/null 2>&1
}

get_bootparam()
{
	local match="$1"
	[ -z "$match" -o ! -r /proc/cmdline ] && return 1

	set -- $(cat /proc/cmdline)
	while [ -n "$1" ]; do
		[ "$1" = "$match" ] && return 0
		case "$1" in
			gentoo=*)
				local params="${1##*=}"
				local IFS=, x=
				for x in $params; do
					[ "$x" = "$match" ] && return 0
				done
				;;
		esac
		shift
	done

	return 1
}

get_bootparam_value()
{
	local match="$1" which_value="$2" sep="$3" result value
	if [ -n "$match" -a -r /proc/cmdline ]; then
		set -- $(cat /proc/cmdline)
		while [ -n "$1" ]; do
			case "$1" in
				$match=*)
					value="${1##*=}"
					case "$which_value" in
						all)
							[ -z "$sep" ] && sep=' '
							if [ -z "$result" ]; then
								result="$value"
							else
								result="${result}${sep}${value}"
							fi
							;;
						last)
							result="$value"
							;;
						*)
							result="$value"
							break
							;;
					esac
					;;
			esac
			shift
		done
	fi
	echo $result
}

need_if_exists()
{
	for x; do
		rc-service --exists "${x}" && need "${x}"
	done
}

# Called from openrc-run.sh or gendepends.sh
_get_containers() {
	local c
	case "${RC_UNAME}" in
	FreeBSD)
		c="-jail"
		;;
	Linux)
		c="-docker -lxc -openvz -rkt -systemd-nspawn -uml -vserver -zone"
		;;
	esac
	echo $c
}

_get_containers_remove() {
	local c
	for x in $(_get_containers); do
		c="${c}!${x} "
	done
	echo $c
}

_depend() {
	depend
	local _rc_svcname=$(shell_var "$RC_SVCNAME") _deptype= _depends=

	# Add any user defined depends
	for _deptype in config:CONFIG need:NEED use:USE want:WANT \
	after:AFTER before:BEFORE \
	provide:PROVIDE keyword:KEYWORD; do
		IFS=:
		set -- $_deptype
		unset IFS
		eval _depends=\$rc_${_rc_svcname}_$1
		[ -z "$_depends" ] && eval _depends=\$rc_$1
		[ -z "$_depends" ] && eval _depends=\$RC_${_rc_svcname}_$2
		[ -z "$_depends" ] && eval _depends=\$RC_$2

		$1 $_depends
	done
}

# Add our sbin to $PATH
case "$PATH" in
	"$RC_LIBEXECDIR"/sbin|"$RC_LIBEXECDIR"/sbin:*);;
	*) PATH="$RC_LIBEXECDIR/sbin:$PATH" ; export PATH ;;
esac
