#!/bin/sh

set -e
set -u

rc_libexecdir="$1"
os="$2"
version="$3"

if [ ${os} != Linux ]; then
	install -d "${DESTDIR}/${rc_libexecdir}"/init.d
fi
install -d "${DESTDIR}/${rc_libexecdir}"/tmp
printf "%s\n" "${version}" > "${DESTDIR}/${rc_libexecdir}"/version
