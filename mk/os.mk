# Copyright 2008 Roy Marples

# Generic definitions

_OS_SH=		uname -s
_OS!= 		${_OS_SH}
OS?= 		${_OS}$(shell ${_OS_SH})
include ${MK}/os-${OS}.mk

RC_LIB=		/$(LIBNAME)/rc

_PREFIX_SH=	if test -n "${PREFIX}" && test "${PREFIX}" != "/"; then echo "-DPREFIX=\\\"${PREFIX}\\\""; else echo ""; fi
_PREFIX!=	${_PREFIX_SH}
CFLAGS+=	${_PREFIX}$(shell ${_PREFIX_SH})

_PKG_PREFIX_SH=	if test -n "${PKG_PREFIX}" && test "${PKG_PREFIX}" != "/" && test "${PKG_PREFIX}" != "${PREFIX}"; then echo "-DPKG_PREFIX=\\\"${PKG_PREFIX}\\\""; else echo ""; fi
_PKG_PREFIX!=	${_PKG_PREFIX_SH}
CFLAGS+=	${_PKG_PREFIX}$(shell ${_PKG_PREFIX_SH})

_LCL_PREFIX_SH=	if test -n "${LOCAL_PREFIX}" && test "${LOCAL_PREFIX}" != "/" && test "${LOCAL_PREFIX}" != "${PREFIX}"; then echo "-DLOCAL_PREFIX=\\\"${LOCAL_PREFIX}\\\""; else echo ""; fi
_LCL_PREFIX!=	${_LCL_PREFIX_SH}
CFLAGS+=	${_LCL_PREFIX}$(shell ${_LCL_PREFIX_SH})
