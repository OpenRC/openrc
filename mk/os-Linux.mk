# Copyright 2008 Roy Marples

SUBOS=		Linux
CFLAGS+=	-D_BSD_SOURCE -D_XOPEN_SOURCE=600
LIBDL=		-Wl,-Bdynamic -ldl
