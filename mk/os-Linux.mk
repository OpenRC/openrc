# Copyright 2008 Roy Marples <roy@marples.name>
# All rights reserved. Released under the 2-clause BSD license.

CFLAGS+=	-D_BSD_SOURCE -D_XOPEN_SOURCE=600
LIBDL=		-Wl,-Bdynamic -ldl
