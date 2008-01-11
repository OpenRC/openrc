# Copyright 2007-2008 Roy Marples
# All rights reserved

# mount $svcdir as something we can write to if it's not rw
# On vservers, / is always rw at this point, so we need to clean out
# the old service state data
if [ "${RC_SVCDIR}" != "/" ] &&  mkdir "${RC_SVCDIR}/.test.$$" 2>/dev/null; then
	rmdir "${RC_SVCDIR}/.test.$$"
	for x in ${RC_SVCDIR:-/lib/rc/init.d}/*; do
		[ -e "${x}" ] || continue
		case ${x##*/} in
			depconfig|deptree|ksoftlevel|rc.log);;
			*) rm -rf "${x}";;
		esac
	done
else
	mount_svcdir
fi

echo "sysinit" > "${RC_SVCDIR}/softlevel"

# sysinit is now done, so allow init scripts to run normally
[ -e /dev/.rcsysinit ] && rm -f /dev/.rcsysinit

exit 0

# vim: set ts=4 :
