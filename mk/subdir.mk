# Recursive rules
# Adapted from FreeBSDs bsd.subdir.mk
# Copyright 2007-2008 Roy Marples <roy@marples.name>

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
	if test -d $$x.${OS}; then \
	${ECHODIR} "===> ${DIRPRFX}$$x.${OS} (${@:realinstall=install})"; \
		cd $$x.${OS}; \
		${MAKE} ${@:realinstall=install} \
		DIRPRFX=${DIRPRFX}$$x.${OS}/ || exit $$?; \
		cd ..; \
	fi; \
	if test -d $$x.${SUBOS}; then \
	${ECHODIR} "===> ${DIRPRFX}$$x.${SUBOS} (${@:realinstall=install})"; \
		cd $$x.${SUBOS}; \
		${MAKE} ${@:realinstall=install} \
		DIRPRFX=${DIRPRFX}$$x.${SUBOS}/ || exit $$?; \
		cd ..; \
	fi; \
done

all:
	${_SUBDIR}
clean:
	${_SUBDIR}
realinstall:
	${_SUBDIR}
install: realinstall ${INSTALLAFTER}
depend:
	${_SUBDIR}
