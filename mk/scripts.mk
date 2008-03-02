# Install rules for our scripts
# Copyright 2007-2008 Roy Marples <roy@marples.name>

_IN_SH=	ls -1 | sed -n -e 's:\.in$$::p' | xargs; echo
_IN!=		${_IN_SH}
OBJS+=		${_IN}$(shell ${_IN_SH})

# We store the contents of the directory for ease of use in Makefiles
_CONTENTS_SH=	ls -1 | grep -v Makefile | sed -e 's:\.in$$::g' | sort -u | xargs
_CONTENTS!=	${_CONTENTS_SH}
CONTENTS=	${_CONTENTS}$(shell ${_CONTENTS_SH})

include ${MK}/sys.mk
include ${MK}/os.mk

# Tweak our shell scripts
.SUFFIXES:	.sh.in .in
.sh.in.sh:
	sed -e 's:@SHELL@:${SH}:g' -e 's:@LIB@:${LIBNAME}:g' -e 's:@PREFIX@:${PREFIX}:g' -e 's:@PKG_PREFIX@:${PKG_PREFIX}:g' $< > $@

.in:
	sed -e 's:@PREFIX@:${PREFIX}:g' -e 's:@PKG_PREFIX@:${PKG_PREFIX}:g' $< > $@

all: ${OBJS}

realinstall: ${BIN} ${CONF} ${CONF_APPEND}
	if test -n "${DIR}"; then ${INSTALL} -d ${DESTDIR}/${PREFIX}${DIR} || exit $$?; fi
	if test -n "${BIN}"; then ${INSTALL} -m ${BINMODE} ${BIN} ${DESTDIR}/${PREFIX}${DIR} || exit $$?; fi
	if test -n "${INC}"; then ${INSTALL} -m ${INCMODE} ${INC} ${DESTDIR}/${PREFIX}${DIR} || exit $$?; fi
	for x in ${CONF}; do \
	 	if ! test -e ${DESTDIR}/${PREFIX}${DIR}/$$x; then \
			${INSTALL} -m ${CONFMODE} $$x ${DESTDIR}/${PREFIX}${DIR} || exit $$?; \
		fi; \
	done
	for x in ${CONF_APPEND}; do \
		if test -e ${DESTDIR}/${PREFIX}${DIR}/$$x; then \
			cat $$x >> ${DESTDIR}/${PREFIX}${DIR}/$$x || exit $$?; \
		else \
	   		${INSTALL} -m ${CONFMODE} $$x ${DESTDIR}/${PREFIX}${DIR} || exit $$?; \
		fi; \
	done

install: all realinstall ${INSTALLAFTER}

# A lot of scripts don't have anything to clean
# Also, some rm implentation require a file argument regardless of error
# so we ensure that it has a bogus argument
CLEANFILES+=	${OBJS}
clean:
	if test -n "${CLEANFILES}"; then rm -f ${CLEANFILES}; fi 
