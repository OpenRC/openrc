# Copyright (c) 2008 Roy Marples <roy@marples.name>
# Released under the 2-clause BSD license.

SFX=		.Linux.in
PKG_PREFIX?=	/usr

CPPFLAGS+=	-D_BSD_SOURCE -D_XOPEN_SOURCE=700
LIBDL=		-Wl,-Bdynamic -ldl

ifeq (${MKSELINUX},yes)
CPPFLAGS+= -DHAVE_SELINUX
LIBSELINUX= -lselinux
LDADD += $(LIBSELINUX)
endif
