# Copyright 1999-2007 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2

# mount $svcdir as something we can write to if it's not rw
# On vservers, / is always rw at this point, so we need to clean out
# the old service state data
if echo 2>/dev/null >"${RC_SVCDIR}/.test.$$" ; then
	rm -rf "${RC_SVCDIR}/.test.$$" \
		$(ls -d1 "${RC_SVCDIR:-/lib/rcscripts/init.d}"/* 2>/dev/null | \
		grep -Ev "/(deptree|ksoftlevel)$")
else
	mount_svcdir
fi

echo "sysinit" > "${RC_SVCDIR}/softlevel"

# sysinit is now done, so allow init scripts to run normally
[ -e /dev/.rcsysinit ] && rm -f /dev/.rcsysinit

exit 0

# vim: set ts=4 :
