LIB=		rc
SHLIB_MAJOR=	1
SRCS=		librc.c librc-daemon.c librc-depend.c librc-misc.c \
		librc-stringlist.c
INCS=		rc.h
VERSION_MAP=	rc.map

LDADD+=		${LIBKVM}

CFLAGS+=	-I../includes

MK=		../../mk
include ${MK}/lib.mk
include ${MK}/cc.mk
include ${MK}/debug.mk

# Massage our header file for our dirs
SED_CMD=	-e 's:@PREFIX@:${PREFIX}:g'
SED_CMD+=	-e 's:@LIB@:${LIBNAME}:g'
SED_CMD+=	-e 's:@SYSCONFDIR@:${SYSCONFDIR}:g'
_PKG_PREFIX_SH=	if test -n "${PKG_PREFIX}" && test "${PKG_PREFIX}" != "/" && test "${PKG_PREFIX}" != "${PREFIX}"; then \
		echo "-e 's:@PKG_PREFIX@:${PKG_PREFIX}:g'"; \
		else \
		echo "-e 's:.*@PKG_PREFIX@.*:\#undef RC_PKG_PREFIX:g'"; \
		fi
_PKG_PREFIX!=	${_PKG_PREFIX_SH}
SED_CMD+=	${_PKG_PREFIX}$(shell ${_PKG_PREFIX_SH})

_LCL_PREFIX_SH=	if test -n "${LOCAL_PREFIX}" && test "${LOCAL_PREFIX}" != "/" && test "${LOCAL_PREFIX}" != "${PREFIX}"; then \
		echo "-e 's:@LOCAL_PREFIX@:${LOCAL_PREFIX}:g'"; \
		else \
		echo "-e 's:@LOCAL_PREFIX@::g'"; \
		fi
_LCL_PREFIX!=	${_LCL_PREFIX_SH}
SED_CMD+=	${_LCL_PREFIX}$(shell ${_LCL_PREFIX_SH})

.SUFFIXES:	.h.in .h
.h.in.h:
	${SED} ${SED_CMD} $< > $@
${SRCS}:	rc.h

CLEANFILES+=	rc.h
