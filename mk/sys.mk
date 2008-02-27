# Generic system definitions
# Copyright 2008 Roy Marples

AR?=			ar
ECHO?=			echo
INSTALL?=		install
RANLIB?=		ranlib
SH=			/bin/sh

PICFLAG?=		-fPIC

BINDIR?=		/sbin
BINMODE?=		0755

INCDIR?=		/usr/include
INCMODE?=		0444

LIBNAME?=		lib
LIBDIR?=		/usr/${LIBNAME}
LIBMODE?=		0444
SHLIBDIR?=		/${LIBNAME}

MANDIR?=		/usr/share/man/man
MANMODE?=		0444

DOCDIR?=		/usr/share/doc
DOCMODE?=		0644	

CONFMODE?=		0644
