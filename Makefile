# Open Run Control Makefile
# Copyright 2006-2007 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2

NAME = openrc
VERSION = 1.0pre1
PKG = $(NAME)-$(VERSION)

SUBDIRS = etc conf.d init.d man net sh src

ifeq ($(OS),)
OS=$(shell uname -s)
ifneq ($(OS),Linux)
OS=BSD
endif
endif

NET_LO = net.lo0
ifeq ($(OS),Linux)
	NET_LO = net.lo
endif

TOPDIR = .
include $(TOPDIR)/default.mk

install::
	# Don't install runlevels if they already exist
	if ! test -d $(DESTDIR)/etc/runlevels ; then \
		(cd runlevels; $(MAKE) install) ; \
		test -d runlevels.$(OS) && (cd runlevels.$(OS); $(MAKE) install) ; \
		$(INSTALL_DIR) $(DESTDIR)/etc/runlevels/single || exit $$? ; \
		$(INSTALL_DIR) $(DESTDIR)/etc/runlevels/nonetwork || exit $$? ; \
	fi
	ln -snf ../../$(RC_LIB)/sh/net.sh $(DESTDIR)/etc/init.d/$(NET_LO) || exit $$?
	ln -snf ../../$(RC_LIB)/sh/functions.sh $(DESTDIR)/etc/init.d || exit $$?
	# Handle lib correctly
	if test $(LIB) != "lib" ; then \
		sed -i'.bak' -e 's,/lib/,/$(LIB)/,g' $(DESTDIR)/$(RC_LIB)/sh/functions.sh || exit $$? ; \
		rm -f $(DESTDIR)/$(RC_LIB)/sh/functions.sh.bak ; \
		sed -i'.bak' -e 's,/lib/,/$(LIB)/,g' $(DESTDIR)/$(RC_LIB)/sh/rc-functions.sh || exit $$? ; \
		rm -f $(DESTDIR)/$(RC_LIB)/sh/rc-functions.sh.bak ; \
	fi

diststatus:
	if test -d .svn ; then \
		svnfiles=`svn status 2>&1 | egrep -v '^(U|P)'` ; \
		if test "x$$svnfiles" != "x" ; then \
			echo "Refusing to package tarball until svn is in sync:" ; \
			echo "$$svnfiles" ; \
			echo "make distforce to force packaging" ; \
			exit 1 ; \
		fi \
	fi 

distit:
	rm -rf /tmp/$(PKG)
	svn export . /tmp/$(PKG)
	$(MAKE) -C /tmp/$(PKG) clean
	tar -C /tmp -cvjpf /tmp/$(PKG).tar.bz2 $(PKG)
	rm -rf /tmp/$(PKG)
	ls -l /tmp/$(PKG).tar.bz2

dist: diststatus distit

.PHONY: layout dist distit diststatus

# vim: set ts=4 :
