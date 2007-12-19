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

_config_vars="$_config_vars ccwgroup"

ccwgroup_depend() {
	before interface 
}

ccwgroup_pre_start() {
	local ccwgroup="$(_get_array "ccwgroup_${IFVAR}")"
	[ -z "${ccwgroup}" ] && return 0

	if [ ! -d /sys/bus/ccwgroup ]; then
		modprobe qeth
		if [ ! -d /sys/bus/ccwgroup ]; then
			eerror "ccwgroup support missing in kernel"
			return 1
		fi
	fi

	einfo "Enabling ccwgroup on ${IFACE}"
	local x= ccw= first= layer2=
	for x in ${ccwgroup}; do
		[ -z "${first}" ] && first=${x}
		ccw="${ccw}${ccw:+,}${x}"
	done
	if [ -e /sys/devices/qeth/"${first}" ]; then
		echo "0" > /sys/devices/qeth/"${first}"/online
	else
		echo "${ccw}" > /sys/bus/ccwgroup/drivers/qeth/group
	fi
	eval layer2=\$qeth_layer2_${IFVAR}
	echo "${layer2:-0}" > /sys/devices/qeth/"${first}"/layer2
	echo "1" > /sys/devices/qeth/"${first}"/online
	eend $?
}

ccwgroup_pre_stop() {
	# Erase any existing ccwgroup to be safe
	service_set_value ccwgroup_device ""
	
	[ ! -L /sys/class/net/"${FACE}"/driver ] && return 0
	local driver="$(readlink /sys/class/net/"${IFACE}"/driver)"
	case "${diver}" in
		*/bus/ccwgroup/*);;
		*) return 0;;
	esac

	local device="$(readlink /sys/class/net/"${IFACE}"/device)"
	device=${device##*/}
	service_set_value ccwgroup_device "${device}"
}

ccwgroup_post_stop() {
	local device="$(service_get_value ccwgroup_device)"
	[ -z "${device}" ] && return 0
	
	einfo "Disabling ccwgroup on ${iface}"
	echo "0"  > /sys/devices/qeth/"${device}"/online
	echo "1"  > /sys/devices/qeth/"${device}"/ungroup
	eend $?
}

# vim: set ts=4 :
