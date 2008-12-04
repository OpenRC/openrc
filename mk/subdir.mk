# Recursive rules
# Adapted from FreeBSDs bsd.subdir.mk
# Copyright 2007-2008 Roy Marples <roy@marples.name>
# All rights reserved. Released under the 2-clause BSD license.

_+_ ?= +
ECHODIR ?= echo 
_SUBDIR = @${_+_}for x in ${SUBDIR}; do \
	if test -d $$x; then \
	${ECHODIR} "===> ${DIRPRFX}$$x (${@:realinstall=install})"; \
		cd $$x; \
		${MAKE} ${@:realinstall=install} \
		DIRPRFX=${DIRPRFX}$$x/ || exit $$?; \
		cd ..; \
	fi; \
done

all:
	${_SUBDIR}
clean:
	@if test -n "${CLEANFILES}"; then echo "rm -f ${CLEANFILES}"; rm -f ${CLEANFILES}; fi
	${_SUBDIR}
realinstall:
	${_SUBDIR}
install: realinstall ${INSTALLAFTER}
check test::
	${_SUBDIR}
depend:
	${_SUBDIR}
ignore:
	${_SUBDIR}
