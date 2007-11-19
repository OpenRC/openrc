# Common makefile settings
# Copyright 2006-2007 Gentoo Foundation

DESTDIR = /
ROOT = /
LIB = lib
RC_LIB = /$(LIB)/rc

INSTALL ?= install

_OS_SH = u=`uname -s`; case "$${u}" in *BSD|DragonFly) echo "BSD";; *) echo "$${u}";; esac
_OS != $(_OS_SH)
OS ?= $(_OS)$(shell $(_OS_SH))

#
# Recursive rules
# Adapted from FreeBSDs bsd.subdir.mk
#
ECHODIR ?= true
_SUBDIR = \
	@for x in $(SUBDIR); do \
		if test -d $$x; then \
			${ECHODIR} "===> ${DIRPRFX}$$x ($@)"; \
			${MAKE} -C $$x $@ DIRPRFX=${DIRPRFX}$$x/; \
		fi; \
		if test -d $$x.${OS}; then \
			${ECHODIR} "===> ${DIRPRFX}$$x.${OS} ($@)"; \
			${MAKE} -C $$x.${OS} $@ DIRPRFX=${DIRPRFX}$$x/; \
		fi; \
	done

all:
	$(_SUBDIR)
clean::
	$(_SUBDIR)
install::
	$(_SUBDIR)

install:: $(BIN) $(CONF) $(CONF_APPEND) $(MAN8)
	if test -n "$(DIR)" ; then $(INSTALL) -d $(DESTDIR)$(DIR) || exit $$?; fi
	if test -n "$(BIN)" ; then $(INSTALL) $(BIN) $(DESTDIR)$(DIR) || exit $$?; fi
	if test -n "$(CONF)" ; then $(INSTALL) -m 0644 $(CONF) $(DESTDIR)$(DIR) || exit $$?; fi
	for x in $(CONF_APPEND) ; do \
	 	if ! test -e $(DESTDIR)$(DIR)/$$x; then \
			$(INSTALL) -m 0644 $$x $(DESTDIR)$(DIR) || exit $$?; \
		fi; \
	done
	for x in $(CONF_APPEND) ; do \
		if test -e $(DESTDIR)$(DIR)/$$x; then \
			cat $$x >> $(DESTDIR)$(DIR)/$$x || exit $$?; \
		else \
	   		$(INSTALL) -m 0644 $$x $(DESTDIR)$(DIR) || exit $$?; \
		fi; \
	done

.PHONY: all clean install
