TOPDIR=			..
include ${TOPDIR}/os.mk

LIB=			einfo
SHLIB_MAJOR=	1
SRCS=			libeinfo.c
INCS=			einfo.h
VERSION_MAP=	einfo.map

SHLIBDIR=		/${LIBNAME}

CFLAGS+=		-I${TOPDIR}

include ${TOPDIR}/cc.mk
include ${TOPDIR}/lib.mk
include ${TOPDIR}/${TERMCAP}.mk
