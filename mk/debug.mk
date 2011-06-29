# rules to enable debugging support
# Copyright (c) 2008 Roy Marples <roy@marples.name>
# Released under the 2-clause BSD license.

_RC_DEBUG_SH=	case "${DEBUG}" in "") echo "";; *) echo "-DRC_DEBUG";; esac
_RC_DEBUG:=	$(shell ${_RC_DEBUG_SH})
CPPFLAGS+=	${_RC_DEBUG}

# Should we enable this with a different flag?
_LD_DEBUG_SH=	case "${DEBUG}" in "") echo "";; *) echo "-Wl,--rpath=../librc -Wl,--rpath=../libeinfo";; esac
_LD_DEBUG:=	$(shell ${_LD_DEBUG_SH})
LDFLAGS+=	${_LD_DEBUG}

_GGDB_SH=	case "${DEBUG}" in "") echo "";; *) echo "-ggdb";; esac
_GGDB:=		$(shell ${_GGDB_SH})
CFLAGS+=	${_GGDB}
