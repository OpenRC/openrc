# rules to build a library
# based on FreeBSD's bsd.prog.mk

# Copyright (c) 2008 Roy Marples <roy@marples.name>
# All rights reserved. Released under the 2-clause BSD license.

OBJS+=			${SRCS:.c=.o}

# Some systems don't include /lib in their standard link path
# so we should embed it if different
# This is currently hardcoded for NetBSD which has two dynamic linkers
# and we need to use the one in /libexec instead of /usr/libexec
_DYNLINK_SH=		if test "${PREFIX}" = "" && test -e /libexec/ld.elf_so; then \
				echo "-Wl,-dynamic-linker=/libexec/ld.elf_so"; \
			else \
				echo ""; \
			fi
_DYNLINK!=		${_DYNLINK_SH}
LDFLAGS+=		${_DYNLINK}$(shell ${_DYNLINK_SH})
LDFLAGS+=		-Wl,-rpath=${PREFIX}/${LIBNAME}
LDFLAGS+=		${PROGLDFLAGS}

CLEANFILES+=		${OBJS} ${PROG}

all: depend ${PROG}

.c.o:
	${CC} ${CFLAGS} ${CPPFLAGS} -c $< -o $@

${PROG}: ${SCRIPTS} ${OBJS}
	${CC} ${LDFLAGS} -o $@ ${OBJS} ${LDADD}

clean:
	rm -f ${CLEANFILES}

extra_depend:

include ${MK}/sys.mk
include ${MK}/os.mk
include ${MK}/depend.mk
include ${MK}/gitignore.mk
