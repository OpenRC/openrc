# OpenRC Makefile
# Copyright (c) 2007-2009 Roy Marples <roy@marples.name>
# All rights reserved. Released under the 2-clause BSD license.

include Makefile.inc

SUBDIR=		conf.d etc init.d local.d man scripts sh src

# Build our old net foo or not
_OLDNET_SH=	case "${MKOLDNET}" in \
		[Yy][Ee][Ss]) echo "net doc";; \
		*) echo "";; \
		esac
_OLDNET:=	$(shell ${_OLDNET_SH})
SUBDIR+=	${_OLDNET}

# Build pkgconfig or not
_PKGCONFIG_SH=	case "${MKPKGCONFIG}" in \
		[Yy][Ee][Ss]|"") echo "pkgconfig";; \
		*) echo "";; \
		esac
_PKGCONFIG:=	$(shell ${_PKGCONFIG_SH})
SUBDIR+=	${_PKGCONFIG}

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
	${INSTALL} -d ${DESTDIR}/${LIBEXECDIR}/init.d
	${INSTALL} -d ${DESTDIR}/${LIBEXECDIR}/tmp
	${ECHO} "${VERSION}${GITVER}" > ${DESTDIR}/${LIBEXECDIR}/version
