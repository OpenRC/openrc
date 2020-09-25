#!/bin/sh

set -e
set -u

rc_libexecdir="$1"
sbindir="$2"
binaries=" halt poweroff rc-sstat reboot shutdown"
for f in $binaries; do
	if [ -x "${DESTDIR}${rc_libexecdir}/bin/${f}"  ]; then
		ln -snf "${DESTDIR}${rc_libexecdir}/bin/${f}" \
			"${DESTDIR}${sbindir}/${f}"
	fi
done
