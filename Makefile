# baselayout Makefile
# Copyright 2006-2007 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
#
# We've moved the installation logic from Gentoo ebuild into a generic
# Makefile so that the ebuild is much smaller and more simple.
# It also has the added bonus of being easier to install on systems
# without an ebuild style package manager.

SUBDIRS = conf.d etc init.d man net sh share src

NAME = baselayout
VERSION = 2.0.0_alpha1

PKG = $(NAME)-$(VERSION)

ARCH = x86
ifeq ($(OS),)
OS=$(shell uname -s)
ifneq ($(OS),Linux)
OS=BSD
endif
endif

BASE_DIRS = /$(LIB)/rcscripts/init.d /$(LIB)/rcscripts/tmp
KEEP_DIRS = /boot /home /mnt /root \
	/usr/local/bin /usr/local/sbin /usr/local/share/doc /usr/local/share/man \
	/var/lock /var/run

ifeq ($(OS),Linux)
	KEEP_DIRS += /dev /sys
	NET_LO = net.lo
endif
ifneq ($(OS),Linux)
	NET_LO = net.lo0
endif

TOPDIR = .
include $(TOPDIR)/default.mk

install::
	# These dirs may not exist from prior versions
	for x in $(BASE_DIRS) ; do \
		$(INSTALL_DIR) $(DESTDIR)$$x || exit $$? ; \
		touch $(DESTDIR)$$x/.keep || exit $$? ; \
	done
	# Don't install runlevels if they already exist
	if ! test -d $(DESTDIR)/etc/runlevels ; then \
		(cd runlevels; $(MAKE) install) ; \
		test -d runlevels.$(OS) && (cd runlevels.$(OS); $(MAKE) install) ; \
		$(INSTALL_DIR) $(DESTDIR)/etc/runlevels/single || exit $$? ; \
		$(INSTALL_DIR) $(DESTDIR)/etc/runlevels/nonetwork || exit $$? ; \
	fi
	ln -snf ../../$(LIB)/rcscripts/sh/net.sh $(DESTDIR)/etc/init.d/$(NET_LO) || exit $$?
	ln -snf ../../$(LIB)/rcscripts/sh/functions.sh $(DESTDIR)/etc/init.d || exit $$?
	# Handle lib correctly
	if test $(LIB) != "lib" ; then \
		sed -i'.bak' -e 's,/lib/,/$(LIB)/,g' $(DESTDIR)/$(LIB)/rcscripts/sh/functions.sh || exit $$? ; \
		rm -f $(DESTDIR)/$(LIB)/rcscripts/sh/functions.sh.bak ; \
	fi

layout:
	# Create base filesytem layout
	for x in $(KEEP_DIRS) ; do \
		$(INSTALL_DIR) $(DESTDIR)$$x || exit $$? ; \
		touch $(DESTDIR)$$x/.keep || exit $$? ; \
	done
	# Special dirs
	install -m 0700 -d $(DESTDIR)/root || exit $$?
	touch $(DESTDIR)/root/.keep || exit $$?
	install -m 1777 -d $(DESTDIR)/var/tmp || exit $$?
	touch $(DESTDIR)/var/tmp/.keep || exit $$?
	install -m 1777 -d $(DESTDIR)/tmp || exit $$?
	touch $(DESTDIR)/tmp/.keep || exit $$?
	# FHS compatibility symlinks stuff
	ln -snf /var/tmp $(DESTDIR)/usr/tmp || exit $$?
	ln -snf share/man $(DESTDIR)/usr/local/man || exit $$?

distcheck:
	if test -d .svn ; then \
		svnfiles=`svn status 2>&1 | egrep -v '^(U|P)'` ; \
		if test "x$$svnfiles" != "x" ; then \
			echo "Refusing to package tarball until svn is in sync:" ; \
			echo "$$svnfiles" ; \
			echo "make distforce to force packaging" ; \
			exit 1 ; \
		fi \
	fi 

distforce:
	svn export . /tmp/$(PKG)
	tar -C /tmp -cvjpf /tmp/$(PKG).tar.bz2 $(PKG)
	du /tmp/$(PKG).tar.bz2

dist: distcheck	distforce

.PHONY: layout dist distcheck distforce

# vim: set ts=4 :
