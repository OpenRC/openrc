# Copyright 2008 Roy Marples

# Generic definitions

_OS_SH_= uname -s
_OS!= ${_OS_SH}
OS?= ${_OS}$(shell ${_OS_SH})

_SUBOS_SH=		case `uname -s` in \
			*BSD|DragonFly) echo "BSD";; \
			*) echo "";; \
			esac
_SUBOS!=		${_SUBOS_SH}
SUBOS?=		${_SUBOS}$(shell ${_SUBOS_SH})

_LIBNAME_SH=	l=`readlink /lib`; case "$$l" in /lib64|lib64) echo "lib64";; *) echo "lib";; esac
_LIBNAME!=		${_LIBNAME_SH}
LIBNAME?=		${_LIBNAME}$(shell ${_LIBNAME_SH})
RC_LIB=			/$(LIBNAME)/rc

_DEF_SH=	case `uname -s` in Linux) echo "-D_XOPEN_SOURCE=600 -D_BSD_SOURCE";; *) echo;; esac
_DEF!=		${_DEF_SH}
CFLAGS+=	${_DEF}$(shell ${_DEF_SH})

_LIBDL_SH=	case `uname -s` in Linux) echo "-Wl,-Bdynamic -ldl";; *) echo;; esac
_LIBDL!=	${_LIBDL_SH}
LIBDL?=		${_LIBDL}$(shell ${_LIBDL_SH})

_LIBKVM_SH=	case `uname -s` in *BSD) echo "-lkvm";; *) echo;; esac
_LIBKVM!=	${_LIBKVM_SH}
LIBKVM?=	${_LIBKVM}$(shell ${_LIBKVM_SH})
