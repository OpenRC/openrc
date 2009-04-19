# OpenRC Makefile
# Copyright 2007-2009 Roy Marples <roy@marples.name>
# All rights reserved. Released under the 2-clause BSD license.

NAME=		openrc
VERSION=	0.4.3
PKG=		${NAME}-${VERSION}

SUBDIR=		conf.d etc init.d man sh src

# Build our old net foo or not
_OLDNET_SH=	case "${MKOLDNET}" in \
		[Yy][Ee][Ss]) echo "net doc";; \
		*) echo "";; \
		esac
_OLDNET!=	${_OLDNET_SH}
SUBDIR+=	${_OLDNET}$(shell ${_OLDNET_SH})

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
