# Common makefile settings
# Copyright 2006-2007 Gentoo Foundation

DESTDIR = /
ROOT = /
LIB = lib

#
# Recursive rules
#

SUBDIRS_ALL     = $(patsubst %,%_all,$(SUBDIRS))
SUBDIRS_CLEAN   = $(patsubst %,%_clean,$(SUBDIRS))
SUBDIRS_INSTALL = $(patsubst %,%_install,$(SUBDIRS))

all::     $(SUBDIRS_ALL)
clean::   $(SUBDIRS_CLEAN)
install:: $(SUBDIRS_INSTALL)

# Hmm ... possible to combine these three and not be ugly ?
%_all:
	$(MAKE) -C $(patsubst %_all,%,$@) all
	if test -d $(patsubst %_all,%,$@).$(OS) ; then $(MAKE) -C $(patsubst %_all,%,$@).$(OS) all ; fi
%_clean:
	$(MAKE) -C $(patsubst %_clean,%,$@) clean
	if test -d $(patsubst %_clean,%,$@).$(OS) ; then $(MAKE) -C $(patsubst %_clean,%,$@).$(OS) clean ; fi
%_install:
	$(MAKE) -C $(patsubst %_install,%,$@) install
	if test -d $(patsubst %_install,%,$@).$(OS) ; then $(MAKE) -C $(patsubst %_install,%,$@).$(OS) install ; fi


#
# Install rules
#

INSTALL_DIR    = install -m 0755 -d
INSTALL_EXE    = install -m 0755
INSTALL_FILE   = install -m 0644
INSTALL_SECURE = install -m 0600

install:: $(EXES) $(FILES) $(FILES_NOEXIST) $(MANS)
	test -n $(DIR) && $(INSTALL_DIR) $(DESTDIR)$(DIR)
	for x in $(EXES)  ; do $(INSTALL_EXE)  $$x $(DESTDIR)$(DIR) || exit $$? ; done
	for x in $(FILES) ; do $(INSTALL_FILE) $$x $(DESTDIR)$(DIR) || exit $$? ; done
	for x in $(FILES_APPEND) ; do if test -e $(DESTDIR)$(DIR)/$$x ; then cat $$x >> $(DESTDIR)$(DIR)/$$x || exit $$? ; else $(INSTALL_FILE) $$x $(DESTDIR)$(DIR) || exit $$? ; fi ; done
	for x in $(FILES_NOEXIST) ; do if ! test -e $(DESTDIR)$(DIR)/$$x ; then $(INSTALL_FILE) $$x $(DESTDIR)$(DIR) || exit $$? ; fi ; done
	for x in $(FILES_SECURE) ; do $(INSTALL_SECURE) $$x $(DESTDIR)$(DIR) || exit $$? ; done
	for x in $(MANS)  ; do \
	    ext=`echo $$x | sed -e 's/^.*\\.//'` ; \
	    $(INSTALL_DIR) $(DESTDIR)$(DIR)/man$$ext || exit $$? ; \
	    $(INSTALL_FILE) $$x $(DESTDIR)$(DIR)/man$$ext || exit $$? ; \
	done

.PHONY: all clean install
