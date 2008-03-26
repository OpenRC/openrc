# Copyright 2008 Roy Marples <roy@marples.name>
# All rights reserved. Released under the 2-clause BSD license.

# Setup some good default CFLAGS
CFLAGS?=	-O2

# Default to using the C99 standard
CSTD?=		c99
_CSTD_SH=	if test -n "${CSTD}"; then echo "-std=${CSTD}"; else echo ""; fi
_CSTD!=		${_CSTD_SH}
CFLAGS+=	${_CSTD}$(shell ${_CSTD_SH})

# Try and use some good cc flags
_CC_FLAGS=	-pedantic -Wall -Wunused -Wimplicit -Wshadow -Wformat=2 \
		-Wmissing-declarations -Wno-missing-prototypes -Wwrite-strings \
		-Wbad-function-cast -Wnested-externs -Wcomment -Winline \
		-Wchar-subscripts -Wcast-align -Wno-format-nonliteral \
		-Wdeclaration-after-statement -Wsequence-point -Wextra
_CC_FLAGS_SH=	for f in ${_CC_FLAGS}; do \
		if ${CC} $$f -S -o /dev/null -xc /dev/null >/dev/null 2>&1; \
		then printf "%s" "$$f "; fi \
		done
_CC_FLAGS!=	${_CC_FLAGS_SH}
CFLAGS+=	${_CC_FLAGS}$(shell ${CC_FLAGS_SH})
