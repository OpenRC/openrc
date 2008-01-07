# Recursive rules
# Adapted from FreeBSDs bsd.subdir.mk
_+_ ?= +
ECHODIR ?= true
_SUBDIR = @${_+_}for x in ${SUBDIR}; do \
	if test -d $$x; then \
		${ECHODIR} "===> ${DIRPRFX}$$x ($@)"; \
		cd $$x; \
		${MAKE} $@ DIRPRFX=${DIRPRFX}$$x/ || exit $$?; \
		cd ..; \
	fi; \
	if test -d $$x.${OS}; then \
		${ECHODIR} "===> ${DIRPRFX}$$x.${OS} ($@)"; \
		cd $$x.${OS}; \
		${MAKE} $@ DIRPRFX=${DIRPRFX}$$x/ || exit $$?; \
		cd ..; \
	fi; \
done

all:
	$(_SUBDIR)
clean:
	$(_SUBDIR)
install:
	$(_SUBDIR)
depend:
	$(_SUBDIR)
