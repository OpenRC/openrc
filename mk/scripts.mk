# Install rules for our scripts
# Copyright 2007-2008 Roy Marples <roy@marples.name>

# We store the contents of the directory for ease of use in Makefiles
_CONTENTS_SH=	ls -1 | grep -v Makefile | xargs
_CONTENTS!=	${_CONTENTS_SH}
CONTENTS=	${_CONTENTS}$(shell ${_CONTENTS_SH})

include ${MK}/sys.mk
include ${MK}/os.mk

all: ${OBJS}

realinstall: ${BIN} ${CONF} ${CONF_APPEND}
	if test -n "${DIR}"; then ${INSTALL} -d ${DESTDIR}${DIR} || exit $$?; fi
	if test -n "${BIN}"; then ${INSTALL} ${BIN} ${DESTDIR}${DIR} || exit $$?; fi
	if test -n "${INC}"; then ${INSTALL} -m 0644 ${INC} ${DESTDIR}${DIR} || exit $$?; fi
	for x in ${CONF}; do \
	 	if ! test -e ${DESTDIR}${DIR}/$$x; then \
			${INSTALL} -m 0644 $$x ${DESTDIR}${DIR} || exit $$?; \
		fi; \
	done
	for x in ${CONF_APPEND}; do \
		if test -e ${DESTDIR}${DIR}/$$x; then \
			cat $$x >> ${DESTDIR}${DIR}/$$x || exit $$?; \
		else \
	   		${INSTALL} -m 0644 $$x ${DESTDIR}${DIR} || exit $$?; \
		fi; \
	done

install: all realinstall ${INSTALLAFTER}

# A lot of scripts don't have anything to clean
# Also, some rm implentation require a file argument regardless of error
# so we ensure that it has a bogus argument
CLEANFILES+=	${OBJS}
clean:
	if test -n "${CLEANFILES}"; then rm -f ${CLEANFILES}; fi 
