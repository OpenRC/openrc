# Generic system definitions
# Copyright (c) 2008 Roy Marples <roy@marples.name>
# Released under the 2-clause BSD license.

AR?=			ar
CP?=			cp
ECHO?=			echo
INSTALL?=		install
RANLIB?=		ranlib
SED?=			sed
SH=			/bin/sh

PREFIX?=
_UPREFIX_SH=		case "${PREFIX}" in "") echo /usr;; *) echo "${PREFIX}";; esac
_UPREFIX:=		$(shell ${_UPREFIX_SH})
UPREFIX=		${_UPREFIX}
LOCAL_PREFIX=		/usr/local

PICFLAG?=		-fPIC

SYSCONFDIR?=		${PREFIX}/etc
INITDIR?=		${SYSCONFDIR}/init.d
CONFDIR?=		${SYSCONFDIR}/conf.d
LOCALDIR?=		${SYSCONFDIR}/local.d

BINDIR?=		${PREFIX}/sbin
BINMODE?=		0755

INCDIR?=		${UPREFIX}/include
INCMODE?=		0444

_LIBNAME_SH=		case `readlink /lib` in /lib64|lib64) echo "lib64";; *) echo "lib";; esac
_LIBNAME:=		$(shell ${_LIBNAME_SH})
LIBNAME?=		${_LIBNAME}
LIBDIR?=		${UPREFIX}/${LIBNAME}
LIBMODE?=		0444
SHLIBDIR?=		${PREFIX}/${LIBNAME}

LIBEXECDIR?=		${PREFIX}/libexec/rc

MANPREFIX?=		${UPREFIX}/share
MANDIR?=		${MANPREFIX}/man
MANMODE?=		0444

DOCDIR?=		${UPREFIX}/share/doc
DOCMODE?=		0644

CONFMODE?=		0644
