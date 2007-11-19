# Open Run Control Makefile
# Copyright 2006-2007 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2

NAME = openrc
VERSION = 1.0
PKG = $(NAME)-$(VERSION)

SUBDIR = conf.d etc init.d man net runlevels sh src

TOPDIR = .
include $(TOPDIR)/default.mk
include $(TOPDIR)/Makefile.$(OS)

install::
	ln -snf ../../$(RC_LIB)/sh/net.sh $(DESTDIR)/etc/init.d/$(NET_LO) || exit $$?
	ln -snf ../../$(RC_LIB)/sh/functions.sh $(DESTDIR)/etc/init.d || exit $$?

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

# vim: set ts=4 :
