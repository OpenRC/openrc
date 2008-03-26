# OpenRC Makefile
# Copyright 2007-2008 Roy Marples <roy@marples.name>
# All rights reserved. Released under the 2-clause BSD license.

NAME=		openrc
VERSION=	0.2
PKG=		${NAME}-${VERSION}

SUBDIR=		conf.d doc etc init.d man net sh src
# We need to ensure that runlevels is done last
SUBDIR+=	runlevels

INSTALLAFTER=	_installafter

MK= 		mk
include ${MK}/sys.mk
include ${MK}/os.mk
include ${MK}/subdir.mk
include ${MK}/dist.mk
include ${MK}/gitignore.mk

_installafter:
	${INSTALL} -d ${DESTDIR}/${PREFIX}/${RC_LIB}/init.d
	${INSTALL} -d ${DESTDIR}/${PREFIX}/${RC_LIB}/tmp
