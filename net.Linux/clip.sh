# Copyright 2005-2007 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2

clip_depend() {
	program /usr/sbin/atmsigd
    before interface
}

_config_vars="$_config_vars clip"

# This starts a service. Albeit atmsigd, ilmid and atmarpd do allow for back-
# grounding through the -b option, its usage causes them to be sensible to
# SIGHUP, which is sent to all daemons when console detaches right after
# startup. This is probably due to the fact that these programs don't detach
# themself from the controlling terminal when backgrounding... The only way I
# see to overcame this is to use the --background option in start-stop-daemon,
# which is reported as a "last resort" method, but it acts correctly about this.
atmclip_svc_start() {
    ebegin "Starting $2 Daemon ($1)"
    start-stop-daemon --start \
		--background \
		--make-pidfile --pidfile "/var/run/$1.pid" \
		--exec "/usr/sbin/$1" -- -l syslog
    eend $?
}

atmclip_svcs_start() {
    einfo "First CLIP instance: starting ATM CLIP daemons"
    eindent

    if [ "${clip_full:-yes}" = "yes" ]; then
		atmclip_svc_start atmsigd "Signaling" && \
		atmclip_svc_start ilmid	  "Integrated Local Management Interface" && \
		atmclip_svc_start atmarpd "Address Resolution Protocol"
    else
		atmclip_svc_start atmarpd "Address Resolution Protocol"
    fi

    local r=$?

    eoutdent
    return ${r}
}

atmclip_svc_stop() {
    ebegin "Stopping $2 Daemon ($1)"
    start-stop-daemon --stop --quiet \
		--pidfile "/var/run/$1.pid" \
		--exec "/usr/sbin/$1"
    eend $?
}

atmclip_svcs_stop() {
    einfo "Last CLIP instance: stopping ATM CLIP daemons"
    eindent

    # Heartake operation!
    sync

    atmclip_svc_stop atmarpd "Address Resolution Protocol"
    if [ "${clip_full:-yes}" = "yes" ]; then
		atmclip_svc_stop ilmid "Integrated Local Management Interface"
		atmclip_svc_stop atmsigd "Signaling"
    fi

    eoutdent
}

are_atmclip_svcs_running() {

	start-stop-daemon --quiet --test --stop --pidfile /var/run/atmarpd.pid || return 1

	if [ "${clip_full:-yes}" = "yes" ]; then
		start-stop-daemon --quiet --test --stop --pidfile /var/run/ilmid.pid || return 1
		start-stop-daemon --quiet --test --stop --pidfile /var/run/atmsigd.pid || return 1
	fi

    return 0
}

clip_pre_start() {
	local clip=
	eval clip=\$clip_${IFVAR}
	[ -z "${clip}" ] && return 0

    if [ ! -r /proc/net/atm/arp ] ; then
		modprobe clip && sleep 2
		if [ ! -r /proc/net/atm/arp ] ; then
	    	eerror "You need first to enable kernel support for ATM CLIP"
	    	return 1
		fi
    fi
	
    local started_here=
    if ! are_atmclip_svcs_running ; then
		atmclip_svcs_start || return 1
		started_here=1
    fi

    if ! _exists ; then
		ebegin "Creating CLIP interface ${IFACE}"
		atmarp -c "${IFACE}"
		if ! eend $? ; then
			[ -z "${started_here}" ] && atmclip_svcs_stop
	    	return 1
		fi
    fi

    return 0
}

clip_post_start() {
	local clip="$(_get_array "clip_${IFVAR}")"
	[ -z "${clip}" ] && return 0

	are_atmclip_svcs_running || return 1

    # The atm tools (atmarpd?) are silly enough that they would not work with
    # iproute2 interface setup as opposed to the ifconfig one.
    # The workaround is to temporarily toggle the interface state from up
    # to down and then up again, without touching its address. This (should)
    # work with both iproute2 and ifconfig.
    _down
    _up

    # Now the real thing: create a PVC with our peer(s).
    # There are cases in which the ATM interface is not yet
    # ready to establish new VCCs. In that cases, atmarp would
    # fail. Here we allow 10 retries to happen every 2 seconds before
    # reporting problems. Also, when no defined VC can be established,
    # we stop the ATM daemons.
    local has_failures= i=
	for i in ${clip} ; do
		local IFS=","
		set -- ${i}
		unset IFS
		local peerip="$1"; shift
		local ifvpivci="$1"; shift
		ebegin "Creating PVC ${ifvpivci} for peer ${peerip}"

		local nleftretries=10 emsg= ecode=
		while [ ${nleftretries} -gt 0 ] ; do
			nleftretries=$((${nleftretries} - 1))
	    	emsg="$(atmarp -s "${peerip}" "${ifvpivci}" "$@" 2>&1)"
	    	ecode=$? && break
	    	sleep 2
		done

		if ! eend ${ecode} ; then
	    	eerror "Creation failed for PVC ${ifvpivci}: ${emsg}"
	    	has_failures=1
		fi
    done

    if [ -n "${has_failures}" ]; then
		clip_pre_stop "${iface}"
		clip_post_stop "${iface}"
		return 1
    else
		return 0
    fi
}

clip_pre_stop() {
    are_atmclip_svcs_running || return 0

	# We remove all the PVCs which may have been created by
	# clip_post_start for this interface. This shouldn't be
	# needed by the ATM stack, but sometimes I got a panic
	# killing CLIP daemons without previously vacuuming
	# every active CLIP PVCs.
	# The linux 2.6's ATM stack is really a mess...
	local itf= t= encp= idle= ipaddr= left=
	einfo "Removing PVCs on this interface"
	eindent
	{
		read left && \
		while read itf t encp idle ipaddr left ; do
			if [ "${itf}" = "${IFACE}" ]; then
				ebegin "Removing PVC to ${ipaddr}"
				atmarp -d "${ipaddr}"
				eend $?
			fi
		done
	} < /proc/net/atm/arp
	eoutdent
}

# Here we should teorically delete the interface previously created in the
# clip_pre_start function, but there is no way to "undo" an interface creation.
# We can just leave the interface down. "ifconfig -a" will still list it...
# Also, here we can stop the ATM CLIP daemons if there is no other CLIP PVC
# outstanding. We check this condition by inspecting the /proc/net/atm/arp file.
clip_post_stop() {
    are_atmclip_svcs_running || return 0

    local itf= left= hasothers=
    {
		read left && \
		while read itf left ; do
	    	if [ "${itf}" != "${IFACE}" ] ; then
				hasothers=1
				break
	    	fi
		done
    } < /proc/net/atm/arp

    if [ -z "${hasothers}" ] ; then
		atmclip_svcs_stop || return 1
    fi
}

# vim: set ts=4 :
