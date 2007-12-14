#!/bin/sh
# Shell wrapper to list our dependencies

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

. /etc/init.d/functions.sh

config() {
	[ -n "$*" ] && echo "${SVCNAME} config $*" >&3
}
need() {
	[ -n "$*" ] && echo "${SVCNAME} ineed $*" >&3
}
use() {
	[ -n "$*" ] && echo "${SVCNAME} iuse $*" >&3
}
before() {
	[ -n "$*" ] && echo "${SVCNAME} ibefore $*" >&3
}
after() {
	[ -n "$*" ] && echo "${SVCNAME} iafter $*" >&3
}
provide() {
	[ -n "$*" ] && echo "${SVCNAME} iprovide $*" >&3
}
keywords() {
	[ -n "$*" ] && echo "${SVCNAME} keywords $*" >&3
}
depend() {
	:
}

for _dir in /etc/init.d /usr/local/etc/init.d; do
	[ -d "${_dir}" ] || continue
	cd "${_dir}"
	for SVCNAME in *; do
		[ -x "${SVCNAME}" ] || continue

		# Only generate dependencies for runscripts
		read one two < "${SVCNAME}"
		[ "${one}" = "#!/sbin/runscript" ] || continue
		unset one two

		SVCNAME=${SVCNAME##*/}
		(
		# Save stdout in fd3, then remap it to stderr
		exec 3>&1 1>&2

		_rc_c=${SVCNAME%%.*}
		if [ -n "${_rc_c}" -a "${_rc_c}" != "${SVCNAME}" ]; then
			[ -e "${_dir}/../conf.d/${_rc_c}" ] && . "${_dir}/../conf.d/${_rc_c}"
		fi
		unset _rc_c

		[ -e "${_dir}/../conf.d/${SVCNAME}" ] && . "${_dir}/../conf.d/${SVCNAME}"

		if . "${_dir}/${SVCNAME}"; then
			echo "${SVCNAME}" >&3
			depend

			# Add any user defined depends
			config ${rc_config} ${RC_CONFIG}
			need ${rc_need} ${RC_NEED}
			use ${rc_use} ${RC_USE}
			before ${rc_before} ${RC_BEFORE}
			after ${rc_after} ${RC_AFTER}
			provide ${rc_provide} ${RC_PROVIDE}
			keywords ${rc_keywords} ${RC_KEYWORDS}
		fi
		)
	done
done

# vim: set ts=4 :
