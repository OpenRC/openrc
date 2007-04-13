#!/sbin/runscript
# Copyright 1999-2007 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2

MODULESDIR="${RC_LIBDIR}/net"
MODULESLIST="${RC_SVCDIR}/nettree"
_config_vars="config routes"

[ -z "${IN_BACKGROUND}" ] && IN_BACKGROUND=false

depend() {
    local IFACE=${SVCNAME#*.}
    local IFVAR=$(echo -n "${IFACE}" | sed -e 's/[^[:alnum:]]/_/g')

    need localmount
	after bootmisc
    provide net
    case "${IFACE}" in
		lo|lo0) ;;
		*)
			after net.lo net.lo0
			local prov=
			eval prov=\$RC_NEED_${IFVAR}
			[ -n "${prov}" ] && need ${prov}
			eval prov=\$RC_USE_${IFVAR}
			[ -n "${prov}" ] && use ${prov}
			eval prov=\$RC_BEFORE_${IFVAR}
			[ -n "${prov}" ] && before ${prov}
			eval prov=\$RC_AFTER_${IFVAR}
			[ -n "${prov}" ] && after ${prov}
			eval prov=\$RC_PROVIDE_${IFVAR}
			[ -n "${prov}" ] && provide ${prov}
			;;
    esac
}

_shell_var() {
    echo -n "$1" | sed -e 's/[^[:alnum:]]/_/g'
}

# Credit to David Leverton for this function which handily maps a bash array
# structure to positional parameters so existing configs work :)
# We'll deprecate arrays at some point though.
_get_array() {
    if [ -n "${BASH}" ] ; then
		case "$(declare -p "$1" 2>/dev/null)" in
	    	"declare -a "*)
	    	echo "set -- \"\${$1[@]}\""
	    	return
	    	;;
		esac
    fi

    echo "eval set -- \"\$$1\""
}

_wait_for_carrier() {
    local timeout= efunc=einfon

    _has_carrier  && return 0

    eval timeout=\$carrier_timeout_${IF_VAR}
    timeout=${timeout:-5}

    [ -n "${RC_EBUFFER}" ] && efunc=einfo
    ${efunc} "Waiting for carrier (${timeout} seconds) "
    while [ ${timeout} -gt 0 ] ; do
		sleep 1
		if _has_carrier ; then
	    	[ -z "${RC_EBUFFER}" ] && echo
			eend 0
	    	return 0
		fi
		timeout=$((${timeout} - 1))
		[ -z "${RC_EBUFFER}" ] && printf "."
    done

    [ -z "${RC_EBUFFER}" ] && echo
	eend 1
    return 1
}

_netmask2cidr() {
    local i= len=0

    local IFS=.
    for i in $1; do
		while [ ${i} != "0" ] ; do
	    	len=$((${len} + ${i} % 2))
	    	i=$((${i} >> 1))
		done
    done

    echo "${len}"
}

_configure_variables() {
    local var= v= t=

    for var in ${_config_vars} ; do
		local v=
		for t in "$@" ; do
		    eval v=\$${var}_${t}
	    	if [ -n "${v}" ] ; then
				eval ${var}_${IFVAR}=\$${var}_${t}
				continue 2
			fi
		done
    done
}

_show_address() {
    einfo "received address $(_get_inet_address "${IFACE}")"
}

# Basically sorts our modules into order and saves the list
_gen_module_list() {
    local x= f=
    if [ -s "${MODULESLIST}" -a "${MODULESLIST}" -nt "${MODULESDIR}" ] ; then
		local update=false
		for x in "${MODULESDIR}"/* ; do
	    	[ -e "${x}" ] || continue
	    	if [ "${x}" -nt "${MODULESLIST}" ] ; then
				update=true
				break
	    	fi
		done
		${update} || return 0
    fi

    einfo "Caching network module dependencies" 
    # Run in a subshell to protect the main script
    (
    after() {
		eval ${MODULE}_after="\"\${${MODULE}_after}\${${MODULE}_after:+ }$*\""
    }

    before() {
		local mod=${MODULE}
		local MODULE=
		for MODULE in "$@" ; do
	    	after "${mod}"
		done
    }

    program() {
		if [ "$1" = "start" -o "$1" = "stop" ] ; then
	    	local s="$1"
	    	shift
	    	eval ${MODULE}_program_${s}="\"\${${MODULE}_program_${s}}\${${MODULE}_program_${s}:+ }$*\""
		else
	    	eval ${MODULE}_program="\"\${${MODULE}_program}\${${MODULE}_program:+ }$*\""
		fi
    }

    provide() {
		eval ${MODULE}_provide="\"\${${MODULE}_provide}\${${MODULE}_provide:+ }$*\""
		local x
		for x in $* ; do
	    	eval ${x}_providedby="\"\${${MODULE}_providedby}\${${MODULE}_providedby:+ }${MODULE}\""
		done
    }

    for MODULE in "${MODULESDIR}"/* ; do
		sh -n "${MODULE}" || continue
		. "${MODULE}" || continue 
		MODULE=${MODULE#${MODULESDIR}/}
		MODULE=${MODULE%.sh}
		eval ${MODULE}_depend
		MODULES="${MODULES} ${MODULE}"
    done

    VISITED=
    SORTED=
    visit() {
		case " ${VISITED} " in
	    	*" $1 "*) return ;;
		esac
		VISITED="${VISITED} $1"

		eval AFTER=\$${1}_after
		for MODULE in ${AFTER} ; do
	    	eval PROVIDEDBY=\$${MODULE}_providedby
	    	if [ -n "${PROVIDEDBY}" ] ; then
				for MODULE in ${PROVIDEDBY} ; do
		    		visit "${MODULE}"
				done
	    	else
				visit "${MODULE}"
	    	fi
		done

		eval PROVIDE=\$${1}_provide
		for MODULE in ${PROVIDE} ; do
	    	visit "${MODULE}"
		done

		eval PROVIDEDBY=\$${1}_providedby
		[ -z "${PROVIDEDBY}" ] && SORTED="${SORTED} $1"
    }

    for MODULE in ${MODULES} ; do
		visit "${MODULE}"
    done

    > "${MODULESLIST}"
    i=0
    for MODULE in ${SORTED} ; do
		eval PROGRAM=\$${MODULE}_program
		eval PROGRAM_START=\$${MODULE}_program_start
		eval PROGRAM_STOP=\$${MODULE}_program_stop
		#for x in ${PROGRAM} ; do
	    #	[ -x "${x}" ] || continue 2
		#done
		eval PROVIDE=\$${MODULE}_provide
		echo "module_${i}='${MODULE}'" >> "${MODULESLIST}"
		echo "module_${i}_program='${PROGRAM}'" >> "${MODULESLIST}"
		echo "module_${i}_program_start='${PROGRAM_START}'" >> "${MODULESLIST}"
		echo "module_${i}_program_stop='${PROGRAM_STOP}'" >> "${MODULESLIST}"
		echo "module_${i}_provide='${PROVIDE}'" >> "${MODULESLIST}"
		i=$((${i} + 1))
    done
    echo "module_${i}=" >> "${MODULESLIST}"
    )

    return 0
}

_load_modules() {
    # Ensure our list is up to date
    _gen_module_list

    local starting=$1 mymods=

    MODULES=
    if [ "${IFACE}" != "lo" -a "${IFACE}" != "lo0" ] ; then
		eval mymods=\$modules_${IFVAR}
		[ -z "${mymods}" ] && mymods=${modules}
    fi

    . "${MODULESLIST}"
    local i=-1 x= mod= f= provides=
    while true ; do
		i=$((${i} + 1))
		eval mod=\$module_${i}
		[ -z "${mod}" ] && break
		[ -e "${MODULESDIR}/${mod}.sh" ] || continue

    	eval set -- \$module_${i}_program
		if [ -n "$1" ] ; then
			x=
			for x in "$@" ; do
				[ -x "${x}" ] && break
			done
			[ -x "${x}" ] || continue
    	fi
		if ${starting} ; then
			eval set -- \$module_${i}_program_start
		else
			eval set -- \$module_${i}_program_stop
		fi
		if [ -n "$1" ] ; then
			x=
			for x in "$@" ; do
				[ -x "${x}" ] && break
			done
			[ -x "${x}" ] || continue
    	fi

		eval provides=\$module_${i}_provide
		if ${starting} ; then
	    	case " ${mymods} " in
				*" !${mod} "*) continue ;;
				*" !${provides} "*) [ -n "${provides}" ] && continue ;;
	    	esac
		fi
		MODULES="${MODULES}${MODULES:+ }${mod}"

		# Now load and wrap our functions
		if ! . "${MODULESDIR}/${mod}.sh" ; then
	    	eend 1 "${SVCNAME}: error loading module \`${mod}'"
	    	exit 1
		fi

		[ -z "${provides}" ] && continue

		# Wrap our provides
		local f=
		for f in pre_start start post_start ; do 
	    	eval "${provides}_${f}() { type ${mod}_${f} >/dev/null 2>/dev/null || return 0; ${mod}_${f} \"\$@\"; }"
		done

		eval module_${mod}_provides="${provides}"
		eval module_${provides}_providedby="${mod}"
    done

    # Wrap our preferred modules
    for mod in ${mymods} ; do
		case " ${MODULES} " in
	    	*" ${mod} "*)
	    		eval x=\$module_${mod}_provides
	    		[ -z "${x}" ] && continue
	    		for f in pre_start start post_start ; do 
					eval "${x}_${f}() { type ${mod}_${f} >/dev/null 2>/dev/null || return 0; ${mod}_${f} \"\$@\"; }"
	    		done
	    		eval module_${x}_providedby="${mod}"
	    		;;
		esac
    done

    # Finally remove any duplicated provides from our list if we're starting
    # Otherwise reverse the list
    local LIST="${MODULES}" p=
    MODULES=
    if ${starting} ; then
		for mod in ${LIST} ; do
	    	eval x=\$module_${mod}_provides
	    	if [ -n "${x}" ] ; then
				eval p=\$module_${x}_providedby
				[ "${mod}" != "${p}" ] && continue
	    	fi
	    	MODULES="${MODULES}${MODULES:+ }${mod}"
		done
    else
		for mod in ${LIST} ; do 
	    	MODULES="${mod}${MODULES:+ }${MODULES}"
		done
    fi

    veinfo "Loaded modules: ${MODULES}"
}

_load_config() {
    eval "$(_get_array "config_${IFVAR}")"
    if [ "${IFACE}" = "lo" -o "${IFACE}" = "lo0" ] ; then
		set -- "127.0.0.1/8" "$@"
    else
		if [ $# -eq 0 ] ; then
	    	ewarn "No configuration specified; defaulting to DHCP"
	    	set -- "dhcp"
		fi
    fi

    # We store our config in an array like vars
    # so modules can influence it
    config_index=0
    for cmd in "$@" ; do
		eval config_${config_index}="'${cmd}'"
		config_index=$((${config_index} + 1))
    done
    # Terminate the list
    eval config_${config_index}=

    config_index=0
    eval $(_get_array fallback_${IFVAR})
    for cmd in "$@" ; do
		eval fallback_${config_index}="'${cmd}'"
		config_index=$((${config_index} + 1))
    done
    # Terminate the list
    eval fallback_${config_index}=

	# Don't set to zero, so any net modules don't have to do anything extra
    config_index=-1
}

start() {
    local IFACE=${SVCNAME#*.} oneworked=false module=
    local IFVAR=$(_shell_var "${IFACE}") cmd= metric=0 our_metric=$metric

    einfo "Bringing up interface ${IFACE}"
    eindent

    if [ -z "${MODULES}" ] ; then
		local MODULES=
		_load_modules true
    fi

    _up 2>/dev/null

    if type preup >/dev/null 2>/dev/null ; then
		ebegin "Running preup"
		eindent
		preup || return 1
		eoutdent
    fi

    for module in ${MODULES} ; do
		if type "${module}_pre_start" >/dev/null 2>/dev/null ; then
	    	if ! ${module}_pre_start ; then
				eend 1
				exit 1
	    	fi
		fi
    done

	if ! _wait_for_carrier ; then
		if service_started devd ; then
			ewarn "no carrier, but devd will start us when we have one"
			mark_service_inactive "${SVCNAME}"
		else
			eerror "no carrier"
		fi
		return 1
	fi

    local config= config_index=
    _load_config
	config_index=0

    if [ -n "${our_metric}" ] ; then
		metric=${our_metric}
    elif [ "${IFACE}" != "lo" -a "${IFACE}" != "lo0" ] ; then
		metric=$((${metric} + $(_ifindex)))
    fi

    while true ; do
		eval config=\$config_${config_index}
		[ -z "${config}" ] && break 

		set -- "${config}"
		ebegin "$1"
		eindent
		case "$1" in
	    	noop)
	    		if [ -n "$(_get_inet_address)" ] ; then
					oneworked=true
					break
	    		fi
	    		;;
	    	null) : ;;
	    	[0-9]*|*:*) _add_address ${config} ;;
	    	*)
	    		if type "${config}_start" >/dev/null 2>/dev/null ; then
					"${config}"_start
	    		else
					eerror "nothing provides \`${config}'"
	    		fi
	    		;;
		esac
		if eend $? ; then
	    	oneworked=true
		else
	    	eval config=\$fallback_${IFVAR}
	    	if [ -n "${config}" ] ; then
				einfo "Trying fallback configuration"
				eval config_${config_index}=\$fallback_${IFVAR}
				eval fallback_${config_index}=
				config_index=$((${config_index} - 1))
	    	fi
		fi
		eoutdent
		config_index=$((${config_index} + 1))
    done

	if ! ${oneworked} ; then
    	if type failup >/dev/null 2>/dev/null ; then
			ebegin "Running failup"
			eindent
			failup
			eoutdent
    	fi
		return 1
	fi

    local hidefirstroute=false first=true routes=
    eval "$(_get_array "routes_${IFVAR}")"
    if [ "${IFACE}" = "lo" -o "${IFACE}" = "lo0" ] ; then
		set -- "127.0.0.0/8 via 127.0.0.1" "$@"
		hidefirstroute=true
    fi
    for cmd in "$@" ; do
		if ${first} ; then
	    	first=false
	    	einfo "Adding routes"
		fi
		eindent
		ebegin "${cmd}"
		# Work out if we're a host or a net if not told
		case "${cmd}" in
	    	*" -net "*|*" -host "*) ;;
	    	*" netmask "*) cmd="-net ${cmd}" ;;
	    	*)
	    		case "${cmd%% *}" in
					*.*.*.*/32) cmd="-host ${cmd}" ;;
					*.*.*.*/*|0.0.0.0|default) cmd="-net ${cmd}" ;;
					*) cmd="-host ${cmd}" ;;
	    		esac
	    		;;
		esac
		if ${hidefirstroute} ; then
	    	_add_route ${cmd} >/dev/null 2>/dev/null
	    	hidefirstroute=false
		else
	    	_add_route ${cmd} >/dev/null
		fi
		eend $?
		eoutdent
    done

    for module in ${MODULES} ; do
		if type "${module}_post_start" >/dev/null 2>/dev/null ; then
	    	if ! ${module}_post_start ; then
			eend 1
			exit 1
	    	fi
		fi
    done

    if type postup >/dev/null 2>/dev/null ; then
		ebegin "Running postup"
		eindent
		postup 
		eoutdent
    fi

    return 0
}

stop() {
    local IFACE=${SVCNAME#*.} module=
    local IFVAR=$(_shell_var "${IFACE}") opts=

    einfo "Bringing down interface ${IFACE}"
    eindent

    if [ -z "${MODULES}" ] ; then
		local MODULES=
		_load_modules false
    fi

    if type predown >/dev/null 2>/dev/null ; then
		ebegin "Running predown"
		eindent
		predown || return 1
		eoutdent
    fi

    for module in ${MODULES} ; do
		if type "${module}_pre_stop" >/dev/null 2>/dev/null ; then
	    	if ! ${module}_pre_stop ; then
			eend 1
			exit 1
	    fi
	fi
    done

    for module in ${MODULES} ; do
		if type "${module}_stop" >/dev/null 2>/dev/null ; then
	    	${module}_stop
		fi
    done

    _delete_addresses "${IFACE}"

    for module in ${MODULES} ; do
		if type "${module}_post_stop" >/dev/null 2>/dev/null ; then
	    	${module}_post_stop
		fi
    done

    [ "${IN_BACKGROUND}" != "true" ] && \
    [ "${IFACE}" != "lo" -a "${IFACE}" != "lo0" ] && \
    _down 2>/dev/null

    [ -x /sbin/resolvconf ] && resolvconf -d "${IFACE}"

    if type postdown >/dev/null 2>/dev/null ; then
		ebegin "Running postdown"
		eindent
		postdown
		eoutdent
    fi

    return 0
}

# vim: set ts=4 :
