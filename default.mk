# Common makefile settings
# We shouldn't use PREFIX as we need to install into /

_LIBNAME_SH= l=`readlink /lib`; case "$$l" in /lib64|lib64) echo "lib64";; *) echo "lib";; esac
_LIBNAME!= ${_LIBNAME_SH}
LIBNAME= ${_LIBNAME}$(shell ${_LIBNAME_SH})
RC_LIB= /${LIBNAME}/rc

INSTALL?= install

_OS_SH= u=`uname -s`; case "$${u}" in *BSD|DragonFly) echo "BSD";; *) echo "$${u}";; esac
_OS!= ${_OS_SH}
OS?= ${_OS}$(shell ${_OS_SH})

# We store the contents of the directory for ease of use in Makefiles
_CONTENTS_SH= ls -1 | grep -v Makefile | xargs
_CONTENTS!= ${_CONTENTS_SH}
CONTENTS= ${_CONTENTS}$(shell ${_CONTENTS_SH})

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
	${_SUBDIR}
clean::
	${_SUBDIR}
depend::
	${_SUBDIR}
install::
	${_SUBDIR}

install:: ${BIN} ${CONF} ${CONF_APPEND}
	if test -n "${DIR}"; then ${INSTALL} -d ${DESTDIR}$(DIR} || exit $$?; fi
	if test -n "${BIN}"; then ${INSTALL} ${BIN} ${DESTDIR}$(DIR} || exit $$?; fi
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

# vim: set ts=4 :
