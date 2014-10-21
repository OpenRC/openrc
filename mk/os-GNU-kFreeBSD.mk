# Copyright (c) 2008 Roy Marples <roy@marples.name>
# Released under the 2-clause BSD license.

# Generic definitions

SFX=		.GNU-kFreeBSD.in
PKG_PREFIX?=	/usr

CPPFLAGS+=	-D_BSD_SOURCE -D_XOPEN_SOURCE=700
LIBDL=		-Wl,-Bdynamic -ldl
LIBKVM?=
