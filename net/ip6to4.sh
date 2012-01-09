# Copyright (c) 2007-2008 Roy Marples <roy@marples.name>
# Released under the 2-clause BSD license.

_config_vars="$_config_vars link suffix relay"

ip6to4_depend()
{
	after interface
	program ip
}

ip6to4_pre_start()
{
	# ALL interfaces run pre_start blocks, not just those with something
	# assigned, so we must check if we need to run on this interface before we
	# do so.
	local config
	eval config=\$config_${IFVAR}
	[ "$config" = "ip6to4" ] || return 0

	case " ${MODULES} " in
		*" ifconfig "*)
			if [ "${IFACE}" != "sit0" ]; then
				eerror "ip6to4 can only work on the sit0 interface using ifconfig"
				eerror "emerge sys-apps/iproute2 to use other interfaces"
				return 1
			fi
	esac

	local host= suffix= relay= addr= iface=${IFACE} config_ip6to4= localip=
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
					: $(( i += 1 ))
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
		config_ip6to4="${config_ip6to4}${config_ip6to4:+ }${ip6}/48"

		if [ -n "${localip}" ]; then
			localip="any"
		else
			localip="${ip}"
		fi
	done

	if [ -z "${config_ip6to4}" ]; then
		eerror "No global IPv4 addresses found on interface ${host}"
		return 1
	fi

	if [ "${IFACE}" != "sit0" ]; then
		ebegin "Creating 6to4 tunnel on ${IFACE}"
		_tunnel add "${IFACE}" mode sit ttl 255 remote any local "${localip}"
		eend $? || return 1
		_up
	fi
	routes_ip6to4="2003::/3 via ::${relay} metric 2147483647"
	service_set_value "config_ip6to4_$IFVAR" "$config_ip6to4"
	service_set_value "routes_ip6to4_$IFVAR" "$routes_ip6to4"
}

ip6to4_start()
{
	local config_ip6to4=$(service_get_value "config_ip6to4_$IFVAR")
	local routes_ip6to4=$(service_get_value "routes_ip6to4_$IFVAR")

	# Now apply our config
	eval config_${config_index}=\'"${config_ip6to4}"\'
	: $(( config_index -= 1 ))

	# Add a route for us, ensuring we don't delete anything else
	local routes="$(_get_array "routes_${IFVAR}")
$routes_ip6to4"
	eval routes_${IFVAR}=\$routes
}
