# Copyright 1999-2007 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2

# Please keep this useable by every shell in portage

RC_GOT_FUNCTIONS="yes"

eindent() {
	RC_EINDENT=$((${RC_EINDENT:-0} + 2))
	[ "${RC_EINDENT}" -gt 40 ] && RC_EINDENT=40
	export RC_EINDENT
}

eoutdent() {
	RC_EINDENT=$((${RC_EINDENT:-0} - 2))
	[ "${RC_EINDENT}" -lt 0 ] && RC_EINDENT=0
	return 0
}

# void esyslog(char* priority, char* tag, char* message)
#
#    use the system logger to log a message
#
esyslog() {
	local pri= tag=

	if [ -x /usr/bin/logger ] ; then
		pri="$1"
		tag="$2"

		shift 2
		[ -z "$*" ] && return 0

		/usr/bin/logger -p "${pri}" -t "${tag}" -- "$*"
	fi

	return 0
}

# Safer way to list the contents of a directory,
# as it do not have the "empty dir bug".
#
# char *dolisting(param)
#
#    print a list of the directory contents
#
#    NOTE: quote the params if they contain globs.
#          also, error checking is not that extensive ...
#
dolisting() {
	local x= y= mylist= mypath="$*"

	# Here we use file globbing instead of ls to save on forking
	for x in ${mypath} ; do
		[ ! -e "${x}" ] && continue

		if [ -L "${x}" -o -f "${x}" ] ; then
			mylist="${mylist} "${x}
		elif [ -d "${x}" ] ; then
			[ "${x%/}" != "${x}" ] && x=${x%/}
			
			for y in "${x}"/* ; do
				[ -e "${y}" ] && mylist="${mylist} ${y}"
			done
		fi
	done

	echo "${mylist# *}"
}

# bool is_older_than(reference, files/dirs to check)
#
#   return 0 if any of the files/dirs are newer than
#   the reference file
#
#   EXAMPLE: if is_older_than a.out *.o ; then ...
is_older_than() {
	local x= ref="$1"
	shift

	for x in "$@" ; do
		[ -e "${x}" ] || continue
		# We need to check the mtime if it's a directory too as the
		# contents may have changed.
		[ "${x}" -nt "${ref}" ] && return 0
		[ -d "${x}" ] && is_older_than "${ref}" "${x}"/* && return 0
	done

	return 1 
}

uniqify() {
    local result=
    while [ -n "$1" ] ; do
		case " ${result} " in
			*" $1 "*) ;;
			*) result="${result} $1" ;;
		esac
		shift
	done
    echo "${result# *}"
}

KV_to_int() {
	[ -z $1 ] && return 1

	local KV_MAJOR=${1%%.*}
	local x=${1#*.}
	local KV_MINOR=${x%%.*}
	x=${1#*.*.}
	local KV_MICRO=${x%%-*}
	local KV_int=$((${KV_MAJOR} * 65536 + ${KV_MINOR} * 256 + ${KV_MICRO} ))

	# We make version 2.2.0 the minimum version we will handle as
	# a sanity check ... if its less, we fail ...
	[ "${KV_int}" -lt 131584 ] && return 1
	
	echo "${KV_int}"
}

# Setup a basic $PATH.  Just add system default to existing.
# This should solve both /sbin and /usr/sbin not present when
# doing 'su -c foo', or for something like:  PATH= rcscript start
case "${PATH}" in
	/lib/rcscripts/bin:/bin:/sbin:/usr/bin:/usr/sbin) ;;
	/lib/rcscripts/bin:/bin:/sbin:/usr/bin:/usr/sbin:*) ;;
	*) export PATH="/lib/rcscripts/bin:/bin:/sbin:/usr/bin:/usr/sbin:${PATH}" ;;
esac

for arg in "$@" ; do
	case "${arg}" in
		--nocolor|--nocolour)
			export RC_NOCOLOR="yes"
			;;
	esac
done

if [ "${RC_NOCOLOR}" != "yes" -a -z "${GOOD}" ] ; then
	eval $(eval_ecolors)
fi

# vim: set ts=4 :
