#!/bin/sh

if [ -z "${BUILD_ROOT}" ] ; then
	printf "%s\n"  "You must export BUILD_ROOT before sourcing this file" >&2
	exit 1
fi

if [ -z "${SOURCE_ROOT}" ] ; then
	printf "%s\n" "You must export SOURCE_ROOT before sourcing this file" >&2
	exit 1
fi

if [ ! -f ${BUILD_ROOT}/sh/functions.sh ] ; then
	printf "%s\n" "functions.sh not yet created !?" >&2
	exit 1
elif ! . ${BUILD_ROOT}/sh/functions.sh; then
	printf "%s\n" "Sourcing functions.sh failed !?" >&2
	exit 1
fi

PATH="${BUILD_ROOT}"/src/einfo:${PATH}
