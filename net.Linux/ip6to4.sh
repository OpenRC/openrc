# Copyright 2004-2007 Gentoo Foundation
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

_config_vars="$_config_vars link suffix relay"

ip6to4_depend() {
	after interface
}

ip6to4_start() {
	case " ${MODULES} " in
		*" ifconfig "*)
			if [ "${IFACE}" != "sit0" ]; then
				eerror "ip6to4 can only work on the sit0 interface using ifconfig"
				eerror "emerge sys-apps/iproute2 to use other interfaces"
				return 1
			fi
	esac

	local host= suffix= relay= addr= iface=${IFACE} new= localip=
	eval host=\$link_${IFVAR}
	if [ -z "${host}" ]; then
		eerror "link_${IFVAR} not set"
		return 1
	fi

	eval suffix=\${suffix_${IFVAR}:-1}
	eval relay=\${relay_${IFVAR}:-192.88.99.1}

	IFACE=${host}
	addrs=$(_get_inet_addresses)
	IFACE=${iface}
	if [ -z "${addrs}" ]; then
		eerror "${host} is not configured with an IPv4 address"
		return 1
	fi

	for addr in ${addrs}; do
		# Strip the subnet
		local ip="${addr%/*}" subnet="${addr#*/}"
		# We don't work on private IPv4 addresses
		case "${ip}" in
			127.*) continue;;
			10.*) continue;;
			192.168.*) continue;;
			172.*)
				local i=16
				while [ ${i} -lt 32 ]; do
					case "${ip}" in
						172.${i}.*) break;;
					esac
					i=$((${i} + 1))
				done
				[ ${i} -lt 32 ] && continue
				;;
		esac
	
		veinfo "IPv4 address on ${host}: ${ip}/${subnet}"
		local ipa= ip6= IFS="${IFS}."
		for i in ${ip}; do
			ipa="${ipa} ${i}"
		done
		unset IFS
		eval ip6="$(printf "2002:%02x%02x:%02x%02x::%s" ${ipa} ${suffix})"
		veinfo "Derived IPv6 address: ${ip6}"

		# Now apply our IPv6 address to our config
		new="${new}${new:+ }${ip6}/16"

		if [ -n "${localip}" ]; then
			localip="any"
		else
			localip="${ip}"
		fi
	done

	if [ -z "${new}" ]; then
		eerror "No global IPv4 addresses found on interface ${host}"
		return 1
	fi

	if [ "${IFACE}" != "sit0" ]; then
		ebegin "Creating 6to4 tunnel on ${IFACE}"
		_tunnel add "${IFACE}" mode sit ttl 255 remote any local "${localip}"
		eend $? || return 1
		_up
	fi
	
	# Now apply our config
	eval config_${config_index}=\'"${new}"\'
	config_index=$((${config_index} - 1))

	# Add a route for us, ensuring we don't delete anything else
	local routes="$(_get_array "routes_${IFVAR}")
2003::/3 via ::${relay} metric 2147483647"
	eval routes_${IFVAR}=\$routes
}
	
# vim: set ts=4 :
