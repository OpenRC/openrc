#!/bin/sh

set -e
set -u

rc_libexecdir="$1"
sbindir="$2"
os="$3"
sysvinit="$4"

if [ "${os}" != Linux ]; then
	install -d "${DESTDIR}/${rc_libexecdir}"/init.d
fi
install -m 644 "${MESON_BUILD_ROOT}/src/shared/version" "${DESTDIR}/${rc_libexecdir}"
if [ "${os}" = Linux ] && [ "${sysvinit}" = yes ]; then
	ln -sf openrc-init "${DESTDIR}/${sbindir}"/init
fi
