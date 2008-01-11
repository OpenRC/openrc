# rules to build a library
# based on FreeBSD's bsd.prog.mk

# Copyright 2008 Roy Marples

OBJS+=			${SRCS:.c=.o}

include ${MK}/sys.mk
# Some systems don't include /lib in their standard link path
# so we should embed it if different
_RPATH_SH=		if test "${SHLIBDIR}" != "/usr/${LIBNAME}"; then \
				echo "-Wl,-rpath-link,${DESTDIR}${SHLIBDIR}:${DESTDIR}/usr/lib"; \
			fi
_RPATH!=		${_RPATH_SH}
LDFLAGS+=		${_RPATH}$(shell ${_RPATH_SH})
LDFLAGS+=		${PROGLDFLAGS}

all: depend ${PROG}

${PROG}: ${SCRIPTS} ${OBJS}
	${CC} ${CFLAGS} ${LDFLAGS} -o $@ ${OBJS} ${LDADD}

clean:
	rm -f ${OBJS} ${PROG} ${CLEANFILES}

include ${MK}/depend.mk
