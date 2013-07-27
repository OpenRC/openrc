#!/bin/sh
# This is a reimplementation of the systemd tmpfiles.d code
# Control creation, deletion, and cleaning of volatile and temporary files
#
# Copyright (c) 2012 Gentoo Foundation
#
# This instance based on the Arch Linux version:
# http://projects.archlinux.org/initscripts.git/tree/arch-tmpfiles
# As of 2012/01/01
#
# See the tmpfiles.d manpage as well:
# http://0pointer.de/public/systemd-man/tmpfiles.d.html
# This script should match the manpage as of 2012/03/12
#

DRYRUN=0

checkprefix() {
	n=$1
	shift
	for x in $@; do
		case $n in
			${x}*) return 0 ;;
	esac
	done
	return 1
}

warninvalid() {
	printf "tmpfiles: ignoring invalid entry on line %d of \`%s'\n" "$LINENUM" "$FILE"
	error=$(( error+1 ))
} >&2

dryrun_or_real() {
	local dryrun=
	[ $DRYRUN -eq 1 ] && dryrun=echo
	$dryrun "$@"
}

relabel() {
	local path
	local paths=$1 mode=$2 uid=$3 gid=$4

	for path in ${paths}; do
		if [ -e "$path" ]; then
			[ $uid != '-' ] && dryrun_or_real chown $CHOPTS "$uid" "$path"
			[ $gid != '-' ] && dryrun_or_real chgrp $CHOPTS "$gid" "$path"
			[ $mode != '-' ] && dryrun_or_real chmod $CHOPTS "$mode" "$path"
			[ -x /sbin/restorecon ] && dryrun_or_real restorecon $CHOPTS "$path"
		fi
	done
}

_b() {
	# Create a block device node if it doesn't exist yet
	local path=$1 mode=$2 uid=$3 gid=$4 age=$5 arg=$6
	if [ ! -e "$path" ]; then
		dryrun_or_real mknod -m $mode $path b ${arg%:*} ${arg#*:}
		dryrun_or_real chown $uid:$gid $path
	fi
}

_c() {
	# Create a character device node if it doesn't exist yet
	local path=$1 mode=$2 uid=$3 gid=$4 age=$5 arg=$6
	if [ ! -e "$path" ]; then
		dryrun_or_real mknod -m $mode $path c ${arg%:*} ${arg#*:}
		dryrun_or_real chown $uid:$gid $path
	fi
}


_f() {
	# Create a file if it doesn't exist yet
	local path=$1 mode=$2 uid=$3 gid=$4 age=$5 arg=$6

	[ $CREATE -gt 0 ] || return 0

	if [ ! -e "$path" ]; then
		dryrun_or_real install -m"$mode" -o"$uid" -g"$gid" /dev/null "$path"
		[ -z "$arg" ] || _w "$@"
	fi
}

_F() {
	# Create or truncate a file
	local path=$1 mode=$2 uid=$3 gid=$4 age=$5 arg=$6

	[ $CREATE -gt 0 ] || return 0

	dryrun_or_real install -m"$mode" -o"$uid" -g"$gid" /dev/null "$path"
	[ -z "$arg" ] || _w "$@"
}

_d() {
	# Create a directory if it doesn't exist yet
	local path=$1 mode=$2 uid=$3 gid=$4

	[ $CREATE -gt 0 ] || return 0

	if [ ! -d "$path" ]; then
		dryrun_or_real install -d -m"$mode" -o"$uid" -g"$gid" "$path"
	fi
}

_D() {
	# Create or empty a directory
	local path=$1 mode=$2 uid=$3 gid=$4

	if [ -d "$path" ] && [ $REMOVE -gt 0 ]; then
		dryrun_or_real find "$path" -mindepth 1 -maxdepth 1 -xdev -exec rm -rf {} +
	fi

	if [ $CREATE -gt 0 ]; then
		dryrun_or_real install -d -m"$mode" -o"$uid" -g"$gid" "$path"
	fi
}

_L() {
	# Create a symlink if it doesn't exist yet
	local path=$1 mode=$2 uid=$3 gid=$4 age=$5 arg=$6
	[ ! -e "$path" ] && dryrun_or_real ln -s "$arg" "$path"
}

_p() {
	# Create a named pipe (FIFO) if it doesn't exist yet
	local path=$1 mode=$2 uid=$3 gid=$4

	[ $CREATE -gt 0 ] || return 0

	if [ ! -p "$path" ]; then
		dryrun_or_real mkfifo -m$mode "$path"
		dryrun_or_real chown "$uid:$gid" "$path"
	fi
}

_x() {
	# Ignore a path during cleaning. Use this type to exclude paths from clean-up as
	# controlled with the Age parameter. Note that lines of this type do not
	# influence the effect of r or R lines. Lines of this type accept shell-style
	# globs in place of of normal path names.
	:
	# XXX: we don't implement this
}

_X() {
	# Ignore a path during cleanup. Use this type to prevent path
	# removal as controled with the age parameter. Note that if path is
	# a directory, the content of the directory is not excluded from
	# clean-up, only the directory itself.
	# Lines of this type accept shell-style globs in place of normal path names.
	:
	# XXX: we don't implement this
}

_r() {
	# Remove a file or directory if it exists. This may not be used to remove
	# non-empty directories, use R for that. Lines of this type accept shell-style
	# globs in place of normal path names.
	local path
	local paths=$1

	[ $REMOVE -gt 0 ] || return 0

	for path in ${paths}; do
		if [ -f "$path" ]; then
			dryrun_or_real rm -f "$path"
		elif [ -d "$path" ]; then
			dryrun_or_real rmdir "$path"
		fi
	done
}

_R() {
	# Recursively remove a path and all its subdirectories (if it is a directory).
	# Lines of this type accept shell-style globs in place of normal path names.
	local path
	local paths=$1

	[ $REMOVE -gt 0 ] || return 0

	for path in ${paths}; do
		[ -d "$path" ] && dryrun_or_real rm -rf --one-file-system "$path"
	done
}

_w() {
	# Write the argument parameter to a file, if it exists.
	local path=$1 mode=$2 uid=$3 gid=$4 age=$5 arg=$6
	if [ -f "$path" ]; then
		if [ $DRYRUN -eq 1 ]; then
			echo "echo \"$arg\" >>\"$path\""
		else
			echo "$arg" >>"$path"
		fi
	fi
}

_z() {
	# Set ownership, access mode and relabel security context of a file or
	# directory if it exists. Lines of this type accept shell-style globs in
	# place of normal path names.
	[ $CREATE -gt 0 ] || return 0

	relabel "$@"
}

_Z() {
	# Recursively set ownership, access mode and relabel security context of a
	# path and all its subdirectories (if it is a directory). Lines of this type
	# accept shell-style globs in place of normal path names.
	[ $CREATE -gt 0 ] || return 0

	CHOPTS=-R relabel "$@"
}

CREATE=0 REMOVE=0 CLEAN=0 VERBOSE=0 DRYRUN=0 error=0 LINENO=0
EXCLUDE=
PREFIX=
FILE=
fragments=
# XXX: The harcoding of /usr/lib/ is an explicit choice by upstream
tmpfiles_dirs='/usr/lib/tmpfiles.d/ /etc/tmpfiles.d/ /run/tmpfiles.d/'
tmpfiles_basenames=''
tmpfiles_d=''
# Build a list of sorted unique basenames
# directories declared later in the tmpfiles_d array will override earlier
# directories, on a per file basename basis.
# `/etc/tmpfiles.d/foo.conf' supersedes `/usr/lib/tmpfiles.d/foo.conf'.
# `/run/tmpfiles/foo.conf' will always be read after `/etc/tmpfiles.d/bar.conf'
for d in ${tmpfiles_dirs} ; do
	[ -d $d ] && for f in ${d}/*.conf ; do
		[ "$f" = "$d/systemd.conf" ] && continue
		[ -f $f ] && tmpfiles_basenames="${tmpfiles_basenames}\n${f##*/}"
	done # for f in ${d}
done # for d in ${tmpfiles_dirs}
tmpfiles_basenames="`printf "${tmpfiles_basenames}\n" | sort | uniq`"

for b in $tmpfiles_basenames ; do
	real_f=''
	for d in $tmpfiles_dirs ; do
		f=${d}/${b}
		[ -f "${f}" ] && real_f=$f
	done
	[ -f "${real_f}" ] && tmpfiles_d="${tmpfiles_d} ${real_f}"
done

while [ $# -gt 0 ]; do
	case $1 in
		--create) CREATE=1 ;;
		--remove) REMOVE=1 ;;
		--clean) CLEAN=1 ;; # TODO: Not implemented
		--verbose) VERBOSE=1 ;;
		--dryrun|--dry-run) DRYRUN=1 ;;
		--exclude-prefix=*) EXCLUDE="${EXCLUDE}${1##--exclude-prefix=} " ;;
		--prefix=*) PREFIX="${PREFIX}${1##--prefix=} " ;;
	esac
	shift
done

if [ $(( CLEAN )) -eq 1 ] ; then
	printf '%s clean mode is not implemented\n' "${0##*/}"
	exit 1
fi

if [ "$CREATE$REMOVE" = '00' ]; then
	printf 'usage: %s [--exclude-prefix=path] [--prefix=path] [--create] [--remove] [--clean] [--verbose] [--dry-run]\n' "${0##*/}"
	exit 1
fi

error=0

# loop through the gathered fragments, sorted globally by filename.
# `/run/tmpfiles/foo.conf' will always be read after `/etc/tmpfiles.d/bar.conf'
for FILE in $tmpfiles_d ; do
	LINENUM=0

	### FILE FORMAT ###
	# XXX: We ignore the 'Age' parameter
	# 1    2              3    4    5    6   7
	# Cmd  Path           Mode UID  GID  Age Argument
	# d    /run/user      0755 root root 10d -
	# Mode, UID, GID, Age, Argument may be omitted!

	# XXX: Upstream says whitespace is NOT permitted in the Path argument.
	# But IS allowed when globs are expanded for the x/r/R/z/Z types.
	while read cmd path mode uid gid age arg; do
		LINENUM=$(( LINENUM+1 ))

		# Unless we have both command and path, skip this line.
		if [ -z "$cmd" -o -z "$path" ]; then
			continue
		fi

		# whine about invalid entries
		case $cmd in
			f|F|w|d|D|p|L|c|b|x|X|r|R|z|Z) ;;
			\#) continue ;;
			*) warninvalid ; continue ;;
		esac

		# fall back on defaults when parameters are passed as '-'
		if [ "$mode" = '-' -o "$mode" = '' ]; then
			case "$cmd" in
				p|f|F) mode=0644 ;;
				d|D) mode=0755 ;;
				z|Z|x|r|R|L) ;;
			esac
		fi

		[ "$uid" = '-' -o "$uid" = '' ] && uid=0
		[ "$gid" = '-' -o "$gid" = '' ] && gid=0
		[ "$age" = '-' -o "$age" = '' ] && age=0
		[ "$arg" = '-' -o "$arg" = '' ] && arg=''
		set -- "$path" "$mode" "$uid" "$gid" "$age" "$arg"

		[ -n "$EXCLUDE" ] && checkprefix $path $EXCLUDE && continue
		[ -n "$PREFIX" ] && ! checkprefix $path $PREFIX && continue
		[ "$VERBOSE" -eq "1" ] && echo _$cmd "$@"
		_$cmd "$@"
		rc=$?
		if [ "${DRYRUN}" -eq "0" ]; then
			[ $rc -ne 0 ] && error=$((error + 1))
		fi
	done <$FILE
done

exit $error

# vim: set ts=2 sw=2 sts=2 noet ft=sh:
