# rules to build a library
# based on FreeBSD's bsd.lib.mk

# Copyright 2008 Roy Marples

LIBNAME?=		lib

SHLIB_NAME=		lib${LIB}.so.${SHLIB_MAJOR}
SHLIB_LINK=		lib${LIB}.so
SHLIBDIR?=		/${LIBNAME}
SONAME?=		${SHLIB_NAME}

OBJS+=			${SRCS:.c=.o}
SOBJS+=			${OBJS:.o=.So}
_LIBS=			lib${LIB}.a ${SHLIB_NAME}

ECHO?=			echo
AR?=			ar
RANLIB?=		ranlib
INSTALL?=		install

PICFLAG?=		-fPIC

INCDIR?=		/usr/include
INCMODE?=		0444

LIBDIR?=		/usr/${LIBNAME}
LIBMODE?=		0444

.SUFFIXES:		.So

.c.So:
	${CC} ${PICFLAG} -DPIC ${CFLAGS} ${CPPFLAGS} -c $< -o $@

all: ${_LIBS}

lib${LIB}.a:	${OBJS} ${STATICOBJS}
	@${ECHO} building static library $@
	${AR} rc $@ $^
	${RANLIB} $@

${SHLIB_NAME}: ${VERSION_MAP}
LDFLAGS+=	-Wl,--version-script=${VERSION_MAP}

${SHLIB_NAME}:	${SOBJS}
	@${ECHO} building shared library $@
	@rm -f $@ ${SHLIB_LINK}
	@ln -fs $@ ${SHLIB_LINK}
	${CC} ${LDFLAGS} -shared -Wl,-x \
	-o $@ -Wl,-soname,${SONAME} \
	${SOBJS} ${LDADD}

install: all
	${INSTALL} -d ${DESTDIR}${LIBDIR}
	${INSTALL} -m ${LIBMODE} lib${LIB}.a ${DESTDIR}${LIBDIR}
	${INSTALL} -d ${DESTDIR}${SHLIBDIR}
	${INSTALL} -m ${LIBMODE} ${SHLIB_NAME} ${DESTDIR}${SHLIBDIR}
	ln -fs ${SHLIBDIR}/${SHLIB_NAME} ${DESTDIR}${LIBDIR}/${SHLIB_LINK}
	${INSTALL} -d ${DESTDIR}${INCDIR}
	for x in ${INCS}; do ${INSTALL} -m ${INCMODE} $$x ${DESTDIR}${INCDIR}; done

clean:
	rm -f ${OBJS} ${SOBJS} ${_LIBS} ${SHLIB_LINK}
