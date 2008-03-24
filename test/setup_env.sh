#!/bin/sh

if [ -z "${top_srcdir}" ] ; then
	echo "You must set top_srcdir before sourcing this file" 1>&2
	exit 1
fi

srcdir=${srcdir:-.}
top_builddir=${top_builddir:-${top_srcdir}}
builddir=${builddir:-${srcdir}}

export LD_LIBRARY_PATH=${top_builddir}/src/libeinfo:${top_builddir}/src/librc:${LD_LIBRARY_PATH}
export PATH=${top_builddir}/src/rc:${PATH}

${MAKE:-make} -s -C ${top_srcdir}/src/rc links

. ${top_srcdir}/sh/functions.sh

if [ $? -ne 0 ] ; then
	echo "Sourcing functions.sh failed !?" 1>&2
	exit 1
fi
