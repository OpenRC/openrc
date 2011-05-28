# Copyright (c) 2008 Roy Marples <roy@marples.name>

# Setup some good default CFLAGS
CFLAGS?=	-O2 -g

# Default to using the C99 standard
CSTD?=		c99
ifneq (${CSTD},)
CFLAGS+=	-std=${CSTD}
endif

# Try and use some good cc flags if we're building from git
# We don't use -pedantic as it will warn about our perfectly valid
# use of %m in our logger.
_CCFLAGS=	-Wall -Wextra -Wimplicit -Wshadow -Wformat=2 \
		-Wmissing-prototypes -Wmissing-declarations \
		-Wmissing-noreturn -Wmissing-format-attribute \
		-Wnested-externs \
		-Winline -Wwrite-strings -Wcast-align -Wcast-qual \
		-Wpointer-arith \
		-Wdeclaration-after-statement -Wsequence-point

# We should be using -Wredundant-decls, but our library hidden proto stuff
# gives loads of warnings. I don't fully understand it (the hidden proto,
# not the warning) so we just silence the warning.

_CC_FLAGS_SH=	for f in ${_CCFLAGS}; do \
		if echo "int main(void) { return 0;} " | \
		${CC} $$f -S -xc -o /dev/null - ; \
		then printf "%s" "$$f "; fi \
		done;
_CC_FLAGS:=	$(shell ${_CC_FLAGS_SH})
CFLAGS+=	${_CC_FLAGS}

include ${MK}/debug.mk
