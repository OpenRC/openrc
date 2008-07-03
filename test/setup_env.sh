#!/bin/sh

if [ -z "${top_srcdir}" ] ; then
	echo "You must set top_srcdir before sourcing this file" 1>&2
	exit 1
fi

srcdir=${srcdir:-.}
top_builddir=${top_builddir:-${top_srcdir}}
builddir=${builddir:-${srcdir}}

if ! . ${top_srcdir}/sh/functions.sh; then
	echo "Sourcing functions.sh failed !?" 1>&2
	exit 1
fi

export LD_LIBRARY_PATH=${top_builddir}/src/libeinfo:${top_builddir}/src/librc:${LD_LIBRARY_PATH}
export PATH=${top_builddir}/src/rc:${PATH}

cd ${top_srcdir}/src/rc
${MAKE:-make} links >/dev/null
cd -

