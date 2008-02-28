# Copyright 2008 Roy Marples

# Generic definitions

_OS_SH=		uname -s
_OS!= 		${_OS_SH}
OS?= 		${_OS}$(shell ${_OS_SH})

_SUBOS_SH=	case `uname -s` in \
		*BSD|DragonFly) echo "BSD";; \
		*) uname -s;; \
		esac
_SUBOS!=	${_SUBOS_SH}
SUBOS?=		${_SUBOS}$(shell ${_SUBOS_SH})

_LIBNAME_SH=	l=`readlink /lib`; case "$$l" in /lib64|lib64) echo "lib64";; *) echo "lib";; esac
_LIBNAME!=	${_LIBNAME_SH}
LIBNAME?=	${_LIBNAME}$(shell ${_LIBNAME_SH})
RC_LIB=		/$(LIBNAME)/rc

_DEF_SH=	case `uname -s` in Linux) echo "-D_BSD_SOURCE -D_XOPEN_SOURCE=600";; *) echo;; esac
_DEF!=		${_DEF_SH}
CFLAGS+=	${_DEF}$(shell ${_DEF_SH})

_LIBDL_SH=	case `uname -s` in Linux) echo "-Wl,-Bdynamic -ldl";; *) echo;; esac
_LIBDL!=	${_LIBDL_SH}
LIBDL?=		${_LIBDL}$(shell ${_LIBDL_SH})

_LIBKVM_SH=	case `uname -s` in *BSD) echo "-lkvm";; *) echo;; esac
_LIBKVM!=	${_LIBKVM_SH}
LIBKVM?=	${_LIBKVM}$(shell ${_LIBKVM_SH})

_PREFIX_SH=	if test -n "${PREFIX}" && test "${PREFIX}" != "/"; then echo "-DPREFIX=\\\"${PREFIX}\\\""; else echo ""; fi
_PREFIX!=	${_PREFIX_SH}
CFLAGS+=	${_PREFIX}$(shell ${_PREFIX_SH})

_PKG_PREFIX_SH=	if test -n "${PKG_PREFIX}" && test "${PKG_PREFIX}" != "/"; then echo "-DPKG_PREFIX=\\\"${PKG_PREFIX}\\\""; else echo ""; fi
_PKG_PREFIX!=	${_PKG_PREFIX_SH}
CFLAGS+=	${_PKG_PREFIX}$(shell ${_PKG_PREFIX_SH})
