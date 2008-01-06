TOPDIR=			..
include $(TOPDIR)/os.mk

LIB=			rc
SHLIB_MAJOR=		1
SRCS=			librc.c librc-daemon.c librc-depend.c librc-misc.c librc-strlist.c
INCS=			rc.h

CPPFLAGS+=		-DLIB=\"${LIBNAME}\"
LDADD+=			${LIBKVM}

SHLIBDIR=		/${LIBNAME}

include $(TOPDIR)/cc.mk
include $(TOPDIR)/lib.mk
