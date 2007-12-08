# Open Run Control Makefile
# Copyright 2006-2007 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2

NAME = openrc
VERSION = 0.1
PKG = $(NAME)-$(VERSION)

SUBDIR = conf.d etc init.d man net runlevels sh src

TOPDIR = .
include $(TOPDIR)/default.mk
include $(TOPDIR)/Makefile.$(OS)

install::
	ln -snf ../../$(RC_LIB)/sh/net.sh $(DESTDIR)/etc/init.d/$(NET_LO) || exit $$?
	ln -snf ../../$(RC_LIB)/sh/functions.sh $(DESTDIR)/etc/init.d || exit $$?
	$(INSTALL) -d $(DESTDIR)$(RC_LIB)/init.d
	$(INSTALL) -d $(DESTDIR)$(RC_LIB)/tmp

clean::
	rm -f *.bz2

dist:
	$(INSTALL) -d /tmp/$(PKG)
	cp -RPp . /tmp/$(PKG)
	(cd /tmp/$(PKG); git clean; $(MAKE) clean)
	rm -rf /tmp/$(PKG)/*.bz2 /tmp/$(PKG)/.git /tmp/$(PKG)/test
	rm -rf /tmp/$(PKG)/.gitignore /tmp/$(PKG)/src/.gitignore
	tar cvjpf $(PKG).tar.bz2 -C /tmp $(PKG) 
	rm -rf /tmp/$(PKG) 
	ls -l $(PKG).tar.bz2

# vim: set ts=4 :
