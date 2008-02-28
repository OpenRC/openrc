# Generic system definitions
# Copyright 2008 Roy Marples

AR?=			ar
ECHO?=			echo
INSTALL?=		install
RANLIB?=		ranlib
SH=			/bin/sh

PREFIX=			
PKG_PREFIX=		/usr/local

PICFLAG?=		-fPIC

BINDIR?=		${PREFIX}/sbin
BINMODE?=		0755

INCDIR?=		${PREFIX}/usr/include
INCMODE?=		0444

LIBNAME?=		lib
LIBDIR?=		${PREFIX}/usr/${LIBNAME}
LIBMODE?=		0444
SHLIBDIR?=		${PREFIX}/${LIBNAME}

MANDIR?=		${PREFIX}/usr/share/man/man
MANMODE?=		0444

DOCDIR?=		${PREFIX}/usr/share/doc
DOCMODE?=		0644	

CONFMODE?=		0644
