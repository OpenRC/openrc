#!/bin/sh

set -e
set -u

rc_libexecdir="$1"
os="$2"

if [ ${os} != Linux ]; then
	install -d "${DESTDIR}/${rc_libexecdir}"/init.d
fi
install -d "${DESTDIR}/${rc_libexecdir}"/tmp
install "${MESON_BUILD_ROOT}/src/version/version" "${DESTDIR}/${rc_libexecdir}"
