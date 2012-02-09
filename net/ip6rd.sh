# Copyright (c) 2011 by Gentoo Foundation
# Released under the 2-clause BSD license.

_config_vars="$_config_vars link prefix suffix ipv4mask relay"

ip6rd_depend()
{
	program ip
	after interface
}

ip6rd_pre_start()
{
	# ALL interfaces run pre_start blocks, not just those with something
	# assigned, so we must check if we need to run on this interface before we
	# do so.
	local config
	eval config=\$config_${IFVAR}
	[ "$config" = "ip6rd" ] || return 0

	case "${MODULES}" in
		*" ifconfig "*)
			eerror "ifconfig is not supported for 6rd"
			eerror "Please emerge sys-apps/iproute2"
			return 1
			;;
	esac

	local host= suffix= relay= addr= iface=${IFACE} config_ip6rd= localip= ipv4mask=
	eval host=\$link_${IFVAR}
	if [ -z "${host}" ]; then
		eerror "link_${IFVAR} not set"
		return 1
	fi

	eval host=\${link_${IFVAR}}
	eval ipv4mask=\${ipv4mask_${IFVAR}:-0}
	eval suffix=\${suffix_${IFVAR}:-1}
	eval relay=\${relay_${IFVAR}}
	eval prefix=\${prefix_${IFVAR}}

	IFACE=${host}
	addrs=$(_get_inet_addresses)
	IFACE=${iface}
	if [ -z "${addrs}" ]; then
		eerror "${host} is not configured with an IPv4 address"
		return 1
	fi
	# TODO: Get this settings from DHCP (Option 212) 
	if [ -z "${prefix}" ]; then
		eerror "prefix_${IFVAR} not set"
		return 1
	fi
	if [ -z "${relay}" ]; then
		eerror "relay_${IFVAR} not set"
		return 1
	fi
	for addr in ${addrs}; do
		# Strip the subnet
		local ip="${addr%/*}" subnet="${addr#*/}"
		# We don't work on private IPv4 addresses
		if _ip6rd_inet_is_private_network "${ip}"
		then
			continue
		fi

		local ip6= ip6_prefix="${prefix%::/*}" ip6_subnet="${prefix#*/}"
		ip6_subnet=$((ip6_subnet + (32-ipv4mask)))
		eval ip6="$(printf "${ip6_prefix}:%s::%s" \
		$(_ip6rd_prefix_shave_bits  ${ip} ${ipv4mask}) ${suffix})"
		veinfo "Derived IPv6 address: ${ip6}"

		# Now apply our IPv6 address to our config
		config_ip6rd="${config_ip6rd}${config_ip6rd:+ }${ip6}/${ip6_subnet}"

		if [ -n "${localip}" ]; then
			localip="any"
		else
			localip="${ip}"
		fi
	done

	if [ -z "${config_ip6rd}" ]; then
		eerror "No global IPv4 addresses found on interface ${host}"
		return 1
	fi

	ebegin "Creating 6rd tunnel ${IFACE}"
	if [ "${IFACE}" != "sit0" ]; then
		_tunnel add "${IFACE}" mode sit ttl 255 remote any local "${localip}"
	fi
	_tunnel 6rd dev "${IFACE}" 6rd-prefix "${prefix}"
	eend $? || return 1
	_up

	routes_ip6rd="2003::/3 via ::${relay} metric 2147483647"
	service_set_value "config_ip6rd_$IFVAR" "$config_ip6rd"
	service_set_value "routes_ip6rd_$IFVAR" "$routes_ip6rd"
}

ip6rd_start()
{
	local config_ip6rd=$(service_get_value "config_ip6rd_$IFVAR")
	local routes_ip6rd=$(service_get_value "routes_ip6rd_$IFVAR")

	# Now apply our config
	eval config_${config_index}=\'"${config_ip6rd}"\'
	: $(( config_index -= 1 ))

	# Add a route for us, ensuring we don't delete anything else
	local routes="$(_get_array "routes_${IFVAR}")
$routes_ip6rd"
	eval routes_${IFVAR}=\$routes
}

_ip6rd_inet_atoi()
{
	local IFS="${IFS}." ipi=0 j=3
	for i in $1 ; do
	       ipi=$(( ipi | i << 8*j-- ))
	done
	echo ${ipi}
}

_ip6rd_inet_itoa()
{
	local ipi=$1
	for i in 0 1 2 3; do
		if [ $i != 3 ] ; then
			printf "%d." $(( (ipi & ~((1<<24)-1)) >> 24 ))
			ipi=$(( (ipi & ((1<<24)-1)) << 8))
		else
			printf "%d\n" $(( (ipi & ~((1<<24)-1)) >> 24 ))
		fi
	done
}

_ip6rd_inet_get_network()
{
	echo $(_ip6rd_inet_itoa $(( ($(_ip6rd_inet_atoi $1) & ((1<<$2)-1) << (32-$2) ) )) )
}

_ip6rd_inet_is_private_network()
{
	if [ "$(_ip6rd_inet_get_network $1 16)" = "192.168.0.0" ]\
	  || [ "$(_ip6rd_inet_get_network $1 8)" = "10.0.0.0" ]\
	  || [ "$(_ip6rd_inet_get_network $1 12)" = "172.16.0.0" ]\
	  || [ "$(_ip6rd_inet_get_network $1 16)" = "169.254.0.0" ]
	then
		return 0;
	fi
	return 1;
}

_ip6rd_prefix_shave_bits()
{
	local ipi=
	ipi=$((  ($(_ip6rd_inet_atoi $1) & (1<<(32-$2))-1) << $2))
	if [ $2 -le 16 ]
	then
		printf "%04x:%0$(( (16-$2>>2)+(($2%4)?1:0) ))x" \
		$((ipi >> 16)) $((ipi & (1<<(16-$2))-1))
	elif [ $2 -lt 32 ]
	then
		printf "%0$(( (32-$2>>2)+(($2%4)?1:0) ))x" \
		$((ipi >> 16))
	fi	
}
