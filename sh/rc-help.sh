#!/bin/sh
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
# OR SVCNAMES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.

. /etc/init.d/functions.sh

myscript=$1
if [ -z "${SVCNAME}" -o -z "$myscript" ]; then
	echo "Please execute an init.d script"
	exit 1
fi

if [ "$2" = "help" ]; then
	BE_VERBOSE="yes"
	NL="\n"
else
	BE_VERBOSE="no"
	NL=
fi

default_commands="describe help start status stop restart zap"
extra_commands="$(. "${myscript}" 2>/dev/null; echo "${extra_commands:-${opts}}")"

printf "Usage: ${HILITE}${SVCNAME}${NORMAL} [ ${GOOD}flags${NORMAL} ] < ${GOOD}options${NORMAL} >

${HILITE}Normal Commands:${NORMAL}"

if yesno ${BE_VERBOSE}; then
printf "
    ${GOOD}describe${NORMAL}
      Describe what the service and any extra options do.
	
    ${GOOD}help${NORMAL}
      This screen - duh.

    ${GOOD}start${NORMAL}
      Start service, as well as the services it depends on (if not already
      started).

    ${GOOD}status${NORMAL}
      Display the current status of the service.

    ${GOOD}stop${NORMAL}
      Stop service, as well as the services that depend on it (if not already
      stopped).

    ${GOOD}restart${NORMAL}
      Restart service, as well as the services that depend on it.

      Note to developers:  If this function is replaced with a custom one,
      'svc_start' and 'svc_stop' should be used instead of 'start' and
      'stop' to restart the service.  This is so that the dependencies
      can be handled correctly.  Refer to the portmap rc-script for an
      example.

    ${GOOD}zap${NORMAL}
      Reset a service that is currently stopped, but still marked as started,
      to the stopped state.  Basically for killing zombie services.

    ${GOOD}status${NORMAL}
      Prints \"status:  started\" if the service is running, else it
      prints \"status:  stopped\".

      Note that if the '--quiet' flag is given, it will return true if the
      service is running, else false.

    ${GOOD}ineed|iuse${NORMAL}
      List the services this one depends on.  Consult the section about
      dependencies for more info on the different types of dependencies.

    ${GOOD}needsme|usesme${NORMAL}
      List the services that depend on this one.  Consult the section about
      dependencies for more info on the different types of dependencies.

    ${GOOD}broken${NORMAL}
      List the missing or broken dependencies of type 'need' this service
      depends on.
"

else

printf "    ${GOOD}${default_commands}${NORMAL}
      Default init.d options.
"

fi

if [ -n "${extra_commands}" ]; then
printf "
${HILITE}Additional Options:${NORMAL}${NL}
    ${GOOD}${extra_commands}${NORMAL}
      Extra options supported by this init.d script.
"
fi

printf "
${HILITE}Flags:${NORMAL}${NL}
    ${GOOD}--ifstarted${NORMAL} Only do actions if service started
    ${GOOD}--nodeps${NORMAL}    Don't stop or start any dependencies  
    ${GOOD}--quiet${NORMAL}
      Suppress output to stdout, except if:${NL}
      1) It is a warning, then output to stdout
      2) It is an error, then output to stderr${NL}
    ${GOOD}--verbose${NORMAL}   Output extra information
    ${GOOD}--debug${NORMAL}     Output debug information
    ${GOOD}--nocolor${NORMAL}   Suppress the use of colors
"

if yesno ${BE_VERBOSE}; then
printf "
${HILITE}Dependencies:${NORMAL}

    This is the heart of the OpenRC, as it determines the order
    in which services gets started, and also to some extend what services
    get started in the first place.

    The following example demonstrates how to use dependencies in
    rc-scripts:

    depend() {
        need foo bar
        use ray
    }

    Here we have foo and bar as dependencies of type 'need', and ray of
    type 'use'.  You can have as many dependencies of each type as needed, as
    long as there is only one entry for each type, listing all its dependencies
    on one line only.

    ${GOOD}need${NORMAL}
      These are all the services needed for this service to start.  If any
      service in the 'need' line is not started, it will be started even if it
      is not in the current, or 'boot' runlevel, and then this service will be
      started.  If any services in the 'need' line fails to start or is
      missing, this service will never be started.

    ${GOOD}use${NORMAL}
      This can be seen as representing optional services this service depends on
      that are not critical for it to start.  For any service in the 'use' line,
      it must be added to the 'boot' or current runlevel to be considered a
      valid 'use' dependency.  It can also be used to determine startup order.

    ${GOOD}before${NORMAL}
      This, together with the 'after' dependency type, can be used to control
      startup order.  In core, 'before' and 'after' do not denote a dependency,
      but should be used for order changes that will only be honoured during
      a change of runlevel.  All services listed will get started *after* the
      current service.  In other words, this service will get started *before*
      all listed services.

    ${GOOD}after${NORMAL}
      All services listed will be started *before* the current service.  Have a
      look at 'before' for more info.

    ${GOOD}provide${NORMAL}
      This is not really a dependency type, rather it will enable you to create
      virtual services.  This is useful if there is more than one version of
      a specific service type, system loggers or crons for instance.  Just
      have each system logger provide 'logger', and make all services in need
      of a system logger depend on 'logger'.  This should make things much more
      generic.

    ${GOOD}config${NORMAL}
      This is not really a dependency type, rather it informs the dependency
      system about config files that may affect the dependencies of the service.
      One example of this is the netmount service which changes its dependencies
      depending on the config of /etc/fstab.

    Note that the 'need', 'use', 'before', and 'after' dependency types accept
    an '*' as an argument.  Having:

    depend() {
    	before *
    }

    will make the service start first in the current runlevel, and:

    depend() {
    	after *
    }

    will make the service the last to start.

    You should however be careful how you use this, as I really will not
    recommend using it with the 'need' or 'use' dependency type ... you have
    been warned!

${HILITE}'net' Dependency and 'net.*' Services:${NORMAL}

    Example:

    depend() {
        need net
    }

    This is a special dependency of type 'need'.  It represents a state where
    a network interface or interfaces besides lo is up and active.  Any service
    starting with 'net.' will be treated as a part of the 'net' dependency,
    if:

    1.  It is part of the 'boot' runlevel
    2.  It is part of the current runlevel

    A few examples are the /etc/init.d/net.eth0 and /etc/init.d/net.lo services.
"
fi

printf "
${HILITE}Configuration files:${NORMAL}
"

if yesno ${BE_VERBOSE}; then
printf "
    There are two files which will be sourced for possible configuration by
    the rc-scripts.  They are (sourced from left to right, top to bottom):
"
fi

printf "     /etc/conf.d/${SVCNAME}
     /etc/conf.d/${SVCNAME}.\${RC_SOFTLEVEL}
     /etc/rc.conf
     \${RC_SOFTLEVEL} denotes the name of the runlevel"

if yesno ${BE_VERBOSE}; then
printf "
    You can add extra dependencies to ${SVCNAME} by adding some variables to
	/etc/conf.d/${SVCNAME}
    rc_need=\"openvpn ntpd\"
    rc_use=\"dns\"

    This makes ${SVCNAME} need openvpn and ntpd, while it just uses dns.

    A good example of this is nfsmount needing openvpn if the nfs mounts in
    /etc/fstab are over the vpn link.
"
fi

if yesno ${BE_VERBOSE}; then
printf "\n
${HILITE}Management:${NORMAL}

    Services are added and removed via the 'rc-update' tool.  Running it without
    arguments should give sufficient help.
"
else
printf "\n
For more info, please run '${myscript} help'.
"
fi

exit 0
