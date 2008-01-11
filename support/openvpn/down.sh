#!/bin/sh
# Copyright 2007-2008 Roy Marples <roy@marples.name>
# All rights reserved. Released under the 2-clause BSD license.

# If we have a service specific script, run this now
[ -x "${SVCNAME}"-down.sh ] && "${SVCNAME}"-down.sh

# Restore resolv.conf to how it was
if type resolvconf >/dev/null 2>&1; then
	resolvconf -d "${dev}"
elif [ -e /etc/resolv.conf-"${dev}".sv ]; then
	# Important that we copy instead of move incase resolv.conf is
	# a symlink and not an actual file
	cp -p /etc/resolv.conf-"${dev}".sv /etc/resolv.conf
	rm -f /etc/resolv.conf-"${dev}".sv
fi

# Re-enter the init script to stop any dependant services
service=/etc/init.d/"${SVCNAME}"
[ ! -x "${service}" ] && service=/usr/local/etc/init.d/"${SVCNAME}"
if [ -x "${service}" ]; then
	if "${service}" --quiet status; then
		export IN_BACKGROUND=YES
		"${service}" --quiet stop
	fi
fi

exit 0
