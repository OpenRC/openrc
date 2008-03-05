# Generic system definitions
# Copyright 2008 Roy Marples

AR?=			ar
ECHO?=			echo
INSTALL?=		install
RANLIB?=		ranlib
SH=			/bin/sh

PREFIX?=	
_UPREFIX_SH=		case "${PREFIX}" in "") echo /usr;; *) echo "${PREFIX}";; esac
_UPREFIX!=		${_UPREFIX_SH}
UPREFIX=		${_UPREFIX}$(shell ${_UPREFIX_SH})
PKG_PREFIX=		/usr/local

PICFLAG?=		-fPIC

SYSCONFDIR?=		/etc
INITDIR?=		${SYSCONFDIR}/init.d
CONFDIR?=		${SYSCONFDIR}/conf.d

BINDIR?=		${PREFIX}/sbin
BINMODE?=		0755

INCDIR?=		${UPREFIX}/include
INCMODE?=		0444

_LIBNAME_SH=		case `readlink /lib` in /lib64|lib64) echo "lib64";; *) echo "lib";; esac
_LIBNAME!=		${_LIBNAME_SH}
LIBNAME?=		${_LIBNAME}$(shell ${_LIBNAME_SH})
LIBDIR?=		${UPREFIX}/${LIBNAME}
LIBMODE?=		0444
SHLIBDIR?=		${PREFIX}/${LIBNAME}

MANDIR?=		${UPREFIX}/share/man/man
MANMODE?=		0444

DOCDIR?=		${UPREFIX}/share/doc
DOCMODE?=		0644	

CONFMODE?=		0644
