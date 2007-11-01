# Copyright 2004-2007 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2

arping_depend() {
	program /sbin/arping /usr/sbin/arping2
	before interface
}

arping_address() {
	local ip=${1%%/*} mac="$2" spoof="$3" foundmac= i= w= opts=

	# We only handle IPv4 addresses
	case "${ip}" in
		0.0.0.0|0) return 1 ;;
		*.*.*.*) ;;
		*) return 1 ;;
	esac

	# We need to bring the interface up to test
	_exists "${iface}" || return 1 
	_up "${iface}"

	eval w=\$arping_wait_${IFVAR}
	[ -z "${w}" ] && w=${arping_wait:-5}

	if type arping2 >/dev/null 2>&1; then
		if [ -n "${spoof}" ]; then
			opts="${opts} -S ${spoof}"
		else
			[ -z "$(_get_inet_address)" ] && opts="${opts} -0"
		fi
		while [ ${w} -gt 0 -a -z "${foundmac}" ]; do
			foundmac="$(arping2 ${opts} -r -c 1 -i "${IFACE}" "${ip}" 2>/dev/null | \
			sed -e 'y/abcdef/ABCDEF/')"
			w=$((${w} - 1))
		done
	else
		[ -z "$(_get_inet_address)" ] && opts="${opts} -D"

		foundmac="$(arping -w "${w}" ${opts} -f -I "${IFACE}" "${ip}" 2>/dev/null | \
		sed -n -e 'y/abcdef/ABCDEF/' -e 's/.*\[\([^]]*\)\].*/\1/p')"
	fi
	[ -z "${foundmac}" ] && return 1
	
	if [ -n "${mac}" ] ; then
		if [ "${mac}" != "${foundmac}" ] ; then
			vewarn "Found ${ip} but MAC ${foundmac} does not match"
			return 1
		fi
	fi

	return 0
}

_arping_in_config() {
	_get_array "config_${IFVAR}" | while read i; do
		[ "${i}" = "arping" ] && return 1
	done
	return 1
}

arping_start() {
	local gateways= x= conf= i=
	einfo "Pinging gateways on ${IFACE} for configuration"

	eval gateways=\$gateways_${IFVAR}
	if [ -z "${gateways}" ] ; then
		eerror "No gateways have been defined (gateways_${IFVAR}=\"...\")"
		return 1
	fi

	eindent
	
	for x in ${gateways}; do
		local IFS=,
		set -- ${x}
		local ip=$1 mac=$2 spoof=$3 extra=
		unset IFS

		if [ -n "${mac}" ] ; then
			mac="$(echo "${mac}" | tr '[:lower:]' '[:upper:]')"
			extra="(MAC ${mac})"
		fi

		vebegin "${ip} ${extra}"
		if arping_address "${ip}" "${mac}" "${spoof}" ; then
			local IFS=.
			for i in ${ip} ; do
				if [ "${#i}" = "2" ] ; then
					conf="${conf}0${i}"
				elif [ "${#i}" = "1" ] ; then
					conf="${conf}00${i}"
				else
					conf="${conf}${i}"
				fi
			done
			unset IFS
			[ -n "${mac}" ] && conf="${conf}_$(echo "${mac}" | sed -e 's/://g')"

			eend 0
			eoutdent
			veinfo "Configuring ${IFACE} for ${ip} ${extra}"
			_configure_variables ${conf}

			# Call the system module as we've aleady passed it by ....
			# And it *has* to be pre_start for other things to work correctly
			system_pre_start

			# Ensure that we have a valid config - ie arping is no longer there
			local IFS="$__IFS"
			for i in $(_get_array "config_${IFVAR}"); do
				if [ "${i}" = "arping" ]; then
					eend 1 "No config found for ${ip} (config_${conf}=\"...\")"
					continue 2
				fi
			done
			unset IFS
			
			_load_config
			return 0
		fi
		veend 1
	done

	eoutdent
	return 1
}

# vim: set ts=4 :
