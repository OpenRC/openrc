# Copyright 1999-2007 Gentoo Foundation
# Copyright 2007 Roy Marples
# All rights reserved

# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.

RC_GOT_FUNCTIONS="yes"

eindent() {
	EINFO_INDENT=$((${EINFO_INDENT:-0} + 2))
	[ "${EINFO_INDENT}" -gt 40 ] && EINFO_INDENT=40
	export EINFO_INDENT
}

eoutdent() {
	EINFO_INDENT=$((${EINFO_INDENT:-0} - 2))
	[ "${EINFO_INDENT}" -lt 0 ] && EINFO_INDENT=0
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
	for x in ${mypath}; do
		[ ! -e "${x}" ] && continue

		if [ -L "${x}" -o -f "${x}" ]; then
			mylist="${mylist} "${x}
		elif [ -d "${x}" ]; then
			[ "${x%/}" != "${x}" ] && x=${x%/}
			
			for y in "${x}"/*; do
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
#   EXAMPLE: if is_older_than a.out *.o; then ...
is_older_than() {
	local x= ref="$1"
	shift

	for x; do
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
    while [ -n "$1" ]; do
		case " ${result} " in
			*" $1 "*);;
			*) result="${result} $1";;
		esac
		shift
	done
    echo "${result# *}"
}

yesno()
{
	[ -z "$1" ] && return 1

	case "$1" in
		[Yy][Ee][Ss]|[Tt][Rr][Uu][Ee]|[Oo][Nn]|1) return 0;;
		[Nn][Oo]|[Ff][Aa][Ll][Ss][Ee]|[Oo][Ff][Ff]|0) return 1;;
	esac

	local value=
	eval value=\$${1}
	case "${value}" in
		[Yy][Ee][Ss]|[Tt][Rr][Uu][Ee]|[Oo][Nn]|1) return 0;;
		[Nn][Oo]|[Ff][Aa][Ll][Ss][Ee]|[Oo][Ff][Ff]|0) return 1;;
		*) vewarn "\$${1} is not set properly"; return 1;;
	esac
}

KV_to_int() {
	[ -z $1 ] && return 1

	local x=${1%%-*}
	local KV_MAJOR=${x%%.*}
	x=${x#*.}
	local KV_MINOR=${x%%.*}
	x=${x#*.}
	local KV_MICRO=${x%%.*}
	local KV_int=$((${KV_MAJOR} * 65536 + ${KV_MINOR} * 256 + ${KV_MICRO} ))

	# We make version 2.2.0 the minimum version we will handle as
	# a sanity check ... if its less, we fail ...
	[ "${KV_int}" -lt 131584 ] && return 1
	
	echo "${KV_int}"
}

_sanitize_path() {
	local IFS=":" p= path=
	for p in ${PATH}; do
		case "${p}" in
			/lib/rc/sbin|/bin|/sbin|/usr/bin|/usr/sbin|/usr/local/bin|/usr/local/sbin);;
			*) path="${path}:${p}";;
		esac
	done

	echo "${path}"
}

# Allow our scripts to support zsh
if [ -n "${ZSH_VERSION}" ]; then
  emulate sh
  NULLCMD=:
  # Zsh 3.x and 4.x performs word splitting on ${1+"$@"}, which
  # is contrary to our usage.  Disable this feature.
  alias -g '${1+"$@"}'='"$@"'
  setopt NO_GLOB_SUBST
fi

# Add our bin to $PATH
export PATH="/lib/rc/bin:/bin:/sbin:/usr/bin:/usr/sbin:/usr/local/bin:/usr/local/sbin$(_sanitize_path "${PATH}")"
unset _sanitize_path

for arg; do
	case "${arg}" in
		--nocolor|--nocolour|-C)
			export EINFO_COLOR="NO"
			;;
	esac
done

if [ -t 1 ] && yesno "${EINFO_COLOR:-YES}"; then
	if [ -z "${GOOD}" ]; then
		eval $(eval_ecolors)
	fi
else
	# We need to have shell stub functions so our init scripts can remember
	# the last ecmd
	for _e in ebegin eend error errorn einfo einfon ewarn ewarnn ewend \
		vebegin veend veinfo vewarn vewend; do
		eval "${_e}() { local _r; /lib/rc/bin/${_e} \"\$@\"; _r=$?; \
		export EINFO_LASTCMD=${_e}; return \$_r; }"
	done
fi

# vim: set ts=4 :
