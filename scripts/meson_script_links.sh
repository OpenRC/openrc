#!/bin/sh

set -e
set -u

rc_libexecdir="$1"
sbindir="$2"
binaries=" halt poweroff rc-sstat reboot shutdown"
for f in $binaries; do
	if [ -x "${DESTDIR}${rc_libexecdir}/bin/${f}"  ]; then
		ln -snf "${rc_libexecdir}/bin/${f}" \
			"${DESTDIR}${sbindir}/${f}"
	fi
done
# sysvinit is active when halt exits
if [ -x "${DESTDIR}${rc_libexecdir}/bin/halt"  ]; then
	ln -snf "${sbindir}/openrc-init" \
		"${DESTDIR}${sbindir}/init"
fi
