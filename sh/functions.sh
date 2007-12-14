# Allow any sh script to work with einfo functions and friends
# We also provide a few helpful functions for other programs to use

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
