# OpenRC Makefile
# Copyright (c) 2007-2009 Roy Marples <roy@marples.name>
# Released under the 2-clause BSD license.

include Makefile.inc

SUBDIR=		conf.d etc init.d local.d man scripts sh src sysctl.d

# Build pkgconfig or not
MKPKGCONFIG?=	yes
ifeq (${MKPKGCONFIG},yes)
SUBDIR+=	pkgconfig
endif

# We need to ensure that runlevels is done last
SUBDIR+=	runlevels

INSTALLAFTER=	_installafter

MK= 		mk
include ${MK}/sys.mk
include ${MK}/os.mk
include ${MK}/subdir.mk
include ${MK}/dist.mk
include ${MK}/git.mk

_installafter:
ifeq (${MKPREFIX},yes)
	${INSTALL} -d ${DESTDIR}/${LIBEXECDIR}/init.d
else ifneq (${OS},Linux)
	${INSTALL} -d ${DESTDIR}/${LIBEXECDIR}/init.d
endif
	${INSTALL} -d ${DESTDIR}/${LIBEXECDIR}/tmp
	${ECHO} "${VERSION}${GITVER}" > ${DESTDIR}/${LIBEXECDIR}/version
