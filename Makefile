# OpenRC Makefile
# Copyright 2007-2008 Roy Marples 
# Distributed under the terms of the GNU General Public License v2

NAME=		openrc
VERSION=	0.1
PKG=		${NAME}-${VERSION}

SUBDIR=		conf.d doc etc init.d man net runlevels sh src

INSTALL?=	install

INSTALLAFTER=	_installafter

MK= 		mk
include ${MK}/os.mk
include ${MK}/subdir.mk
include ${MK}/dist.mk

_installafter:
	${INSTALL} -d ${DESTDIR}${RC_LIB}/init.d
	${INSTALL} -d ${DESTDIR}${RC_LIB}/tmp
