# rules to build a library
# based on FreeBSD's bsd.prog.mk

# Copyright 2008 Roy Marples

OBJS+=			${SRCS:.c=.o}

include ${MK}/sys.mk
# Some systems don't include /lib in their standard link path
# so we should embed it if different
# This is currently hardcoded for NetBSD which has two dynamic linkers
# and we need to use the one in /libexec instead of /usr/libexec
_DYNLINK_SH=		if test -e /libexec/ld.elf_so; then \
				echo "-Wl,-dynamic-linker=/libexec/ld.elf_so"; \
			fi
_DYNLINK!=		${_DYNLINK_SH}
LDFLAGS+=		${_DYNLINK}$(shell ${_DYNLINK_SH})
LDFLAGS+=		-Wl,-rpath=/${LIBNAME} -L/${LIBNAME}
LDFLAGS+=		${PROGLDFLAGS}

all: depend ${PROG}

${PROG}: ${SCRIPTS} ${OBJS}
	${CC} ${CFLAGS} ${LDFLAGS} -o $@ ${OBJS} ${LDADD}

clean:
	rm -f ${OBJS} ${PROG} ${CLEANFILES}

include ${MK}/depend.mk
