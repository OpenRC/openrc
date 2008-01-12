MK=			../../mk
include ${MK}/os.mk

LIB=			einfo
SHLIB_MAJOR=		1
SRCS=			libeinfo.c
INCS=			einfo.h
VERSION_MAP=		einfo.map

SHLIBDIR=		/${LIBNAME}

CFLAGS+=		-I../includes

include ${MK}/cc.mk
include ${MK}/lib.mk
include ${MK}/${MKTERMCAP}.mk
