MK=		../../mk
include ${MK}/os.mk

LIB=		rc
SHLIB_MAJOR=	1
SRCS=		librc.c librc-daemon.c librc-depend.c librc-misc.c \
		librc-stringlist.c
INCS=		rc.h
VERSION_MAP=	rc.map

CFLAGS+=	-DLIB=\"${LIBNAME}\"
CFLAGS+=	-DSYSCONFDIR=\"${SYSCONFDIR}\"
LDADD+=		${LIBKVM}

CFLAGS+=	-I../includes

include ${MK}/cc.mk
include ${MK}/lib.mk
