#!/bin/sh
# Copyright (c) 2007-2008 Roy Marples <roy@marples.name>
# Released under the 2-clause BSD license.

# If we have a service specific script, run this now
[ -x "${RC_SVCNAME}"-down.sh ] && "${RC_SVCNAME}"-down.sh

# Restore resolv.conf to how it was
if command -v resolvconf >/dev/null 2>&1; then
	resolvconf -d "${dev}"
elif [ -e /etc/resolv.conf-"${dev}".sv ]; then
	# Important that we copy instead of move incase resolv.conf is
	# a symlink and not an actual file
	cp -p /etc/resolv.conf-"${dev}".sv /etc/resolv.conf
	rm -f /etc/resolv.conf-"${dev}".sv
fi

# Re-enter the init script to stop any dependant services
if [ -x "${RC_SERVICE}" ]; then
	if "${RC_SERVICE}" --quiet status; then
		IN_BACKGROUND=YES
		export IN_BACKGROUND
		"${RC_SERVICE}" --quiet stop
	fi
fi

exit 0
