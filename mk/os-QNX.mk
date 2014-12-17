# Copyright (c) 2008 Roy Marples <roy@marples.name>
# Released under the 2-clause BSD license.

# Generic definitions

#PKG_PREFIX?=	/usr/local
SFX=		.QNX.in

# When QNX is building with std=c99, we need to manually re-enable "extensions"
# ... such as the POSIX APIs.
CFLAGS+=-D__EXT

# qcc doesn't understand -fPIC - use -shared instead
PICFLAG=-shared

# QNX includes a build of ncurses; we always uses the PIC compile
MKTERMCAP=ncurses
LIBTERMCAP=-lncursesS
