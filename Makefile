# OpenRC Makefile
# Copyright 2007-2008 Roy Marples 
# Distributed under the terms of the GNU General Public License v2

NAME=		openrc
VERSION=	0.1
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

_installafter:
	${INSTALL} -d ${DESTDIR}/${PREFIX}/${RC_LIB}/init.d
	${INSTALL} -d ${DESTDIR}/${PREFIX}/${RC_LIB}/tmp
