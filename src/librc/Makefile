MK=		../../mk
include ${MK}/os.mk

LIB=		rc
SHLIB_MAJOR=	1
SRCS=		librc.c librc-daemon.c librc-depend.c librc-misc.c \
		librc-strlist.c
INCS=		rc.h
VERSION_MAP=	rc.map

CFLAGS+=	-DLIB=\"${LIBNAME}\"
LDADD+=		${LIBKVM}

CFLAGS+=	-I../includes

include ${MK}/cc.mk
include ${MK}/lib.mk
