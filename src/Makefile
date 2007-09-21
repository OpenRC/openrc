# Copyright 1999-2007 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2

# We should strive to keep this Makefile working with the default make
# shipped with the OS's we support. Mainly because I'm lazy and just want
# to type make instead of gmake, but also so that other distros can pick
# it up and not rely on GNU Make.

# NOTE:- FreeBSD and DragonFly have no way of optionally including files
# that works with GNU make and vice versa. NetBSD and OpenBSD makes do.
# You can get a patch from
# http://www.freebsd.org/cgi/query-pr.cgi?pr=standards/116081
# to fix this. We include this patch with Gentoo/FreeBSD :)

CC ?= gcc
CFLAGS += -O2 -pipe
LDFLAGS += -L.

# GNU Make way of detecting gcc flags we can use
check_gcc=$(shell if $(CC) $(1) -S -o /dev/null -xc /dev/null >/dev/null 2>&1; \
	  then echo "$(1)"; else echo "$(2)"; fi)

# pmake check for extra cflags 
WEXTRA != for x in -Wdeclaration-after-statement -Wsequence-point -Wextra; do \
	if $(CC) -Wdeclaration-after-statement -S -o /dev/null -xc /dev/null >/dev/null 2>&1; \
	then echo -n "$$x "; fi \
	done

# Loads of nice flags to ensure our code is good
CFLAGS += -pedantic -std=c99 \
    -Wall -Wunused -Wimplicit -Wshadow -Wformat=2 \
    -Wmissing-declarations -Wno-missing-prototypes -Wwrite-strings \
    -Wbad-function-cast -Wnested-externs -Wcomment -Winline \
    -Wchar-subscripts -Wcast-align -Wno-format-nonliteral \
    $(call check_gcc, -Wdeclaration-after-statement) \
    $(call check_gcc, -Wsequence-point) \
    $(call check_gcc, -Wextra) $(WEXTRA)

# For debugging. -Werror is pointless due to ISO C issues with dlsym
#CFLAGS += -ggdb

DESTDIR =
LIB = lib
RC_LIB = /$(LIB)/rc

# Set PAM = pam for pam support
PAM = 

LIBEINFOSOVER = 0
LIBEINFOSO = libeinfo.so.$(LIBRCSOVER)
LIBEINFOOBJS= libeinfo.o
LDLIBS_LIBEINFO = $(LDLIBS)

LIBRCSOVER = 0
LIBRCSO = librc.so.$(LIBRCSOVER)
LIBRCOBJS = librc.o librc-depend.o librc-daemon.o librc-misc.o librc-strlist.o
LDLIBS_LIBRC = -leinfo

RCOBJS = checkown.o env-update.o fstabinfo.o mountinfo.o \
		 rc-depend.o rc-plugin.o rc-status.o rc-update.o runscript.o \
		 start-stop-daemon.o rc.o
LDLIBS_RC = $(LDLIBS_LIBRC) -lrc -lutil

LIB_TARGETS = $(LIBEINFOSO) $(LIBRCSO)
SBIN_TARGETS = rc
SYS_WHITELIST = env_whitelist

TARGET = $(LIB_TARGETS) $(BIN_TARGETS) $(SBIN_TARGETS) $(PRIV_BIN_TARGETS)

RCLINKS = einfon einfo ewarnn ewarn eerrorn eerror ebegin eend ewend \
	      eindent eoutdent esyslog eval_ecolors \
	      veinfo vewarn vebegin veend vewend veindent veoutdent \
	      service_starting service_inactive service_started \
	      service_stopping service_stopped \
	      service_inactive service_wasinactive \
	      service_coldplugged \
	      mark_service_starting mark_service_inactive mark_service_started \
	      mark_service_stopping mark_service_stopped \
	      mark_service_inactive mark_service_wasinactive \
	      mark_service_coldplugged \
	      get_options save_options rc-abort rc-depend \
	      is_runlevel_start is_runlevel_stop service_started_daemon \
		  checkown fstabinfo mountinfo
BINLINKS = rc-status
SBINLINKS = env-update rc-update runscript start-stop-daemon

# We also define _BSD_SOURCE so both Linux and the BSDs get a few
# handy functions which makes our lives a lot easier
CPPFLAGS += -DLIB=\"$(LIB)\"

# IMPORTANT!!!
# Ensure that we don't ship with a .svn directory to avoid RPATH security
# issues. However, this does ease development a little
_SVN_LDFLAGS_SH = if test -d .svn; then echo "-Wl,-rpath ."; else echo ""; fi
_SVN_LDFLAGS != $(_SVN_LDFLAGS_SH)
LDFLAGS += $(_SVN_LDFLAGS)$(shell $(_SVN_LDFLAGS_SH))

# Load an optional OS Makefile
_OS_SH = u=`uname -s`; case "$${u}" in *BSD|DragonFly) echo "BSD";; *) echo "$${u}";; esac
_OS != $(_OS_SH)
OS ?= $(_OS)$(shell $(_OS_SH))
-include Makefile.$(OS)
-include Makefile.$(PAM)

all: .depend $(TARGET)

$(LIBEINFOOBJS):
	$(CC) $(CPPFLAGS) $(CFLAGS) -fPIC -c $<
$(LIBEINFOSO): $(LIBEINFOOBJS) 
	$(CC) $(LDFLAGS) -fPIC -shared -Wl,-soname,$(LIBEINFOSO) -o $(LIBEINFOSO) $(LIBEINFOOBJS) $(LDLIBS) $(LDLIBS_LIBEINFO)
	ln -sf $(LIBEINFOSO) libeinfo.so

$(LIBRCOBJS):
	$(CC) $(CPPFLAGS) $(CFLAGS) -fPIC -c $<
$(LIBRCSO): $(LIBRCOBJS) $(LIBEINFOSO) 
	$(CC) $(LDFLAGS) -fPIC -shared -Wl,-soname,$(LIBRCSO) -o $(LIBRCSO) $(LIBRCOBJS) $(LDLIBS) $(LDLIBS_LIBRC)
	ln -sf $(LIBRCSO) librc.so

$(RCOBJS):
	$(CC) $(CPPFLAGS) $(CPPFLAGS_SSD) $(CFLAGS) -c $<
rc: $(LIBEINFOSO) $(LIBRCSO) $(RCOBJS)
	$(CC) $(LDFLAGS) -o rc $(RCOBJS) $(LDLIBS) $(LDLIBS_RC)

links: rc
	for x in $(BINLINKS) $(SBINLINKS) $(RCLINKS) $(RCPRIVLINKS); do ln -sf rc $$x; done

install: $(TARGET)
	install -m 0755 -d $(DESTDIR)/$(LIB)
	install -m 0755 $(LIB_TARGETS) $(DESTDIR)/$(LIB)
	ln -sf $(LIBEINFOSO) $(DESTDIR)/$(LIB)/libeinfo.so
	ln -sf $(LIBRCSO) $(DESTDIR)/$(LIB)/librc.so
	install -m 0755 -d $(DESTDIR)/usr/include
	install -m 0644 einfo.h rc.h $(DESTDIR)/usr/include
	install -m 0755 -d $(DESTDIR)/bin
	install -m 0755 -d $(DESTDIR)/sbin
	install -m 0755 $(SBIN_TARGETS) $(DESTDIR)/sbin
	ln -sf rc-update $(DESTDIR)/sbin/update-rc
	install -m 0755 -d $(DESTDIR)/$(RC_LIB)/conf.d
	install -m 0644 $(SYS_WHITELIST) $(DESTDIR)/$(RC_LIB)/conf.d
	install -m 0755 -d $(DESTDIR)/$(RC_LIB)/bin
	for x in $(BINLINKS); do ln -sf ../sbin/rc $(DESTDIR)/bin/$$x; done
	for x in $(SBINLINKS); do ln -sf rc $(DESTDIR)/sbin/$$x; done
	for x in $(RCLINKS); do ln -sf ../../../sbin/rc $(DESTDIR)/$(RC_LIB)/bin/$$x; done
	if test "$(PAM)" = "pam" ; then \
		install -m 0755 -d $(DESTDIR)/etc/pam.d ; \
		install -m 0644 start-stop-daemon.pam $(DESTDIR)/etc/pam.d/start-stop-daemon ; \
	fi

clean:
	rm -f $(TARGET) $(BINLINKS) $(SBINLINKS) $(RCLINKS) $(RCPRIVLINKS)
	rm -f *.o *~ *.core *.so .depend

-include .depend
_DEPS != ls *.c *.h
.depend: $(_DEPS)$(wildcard *.c *.h)
	$(CC) $(CPPFLAGS) -MM *.c > .depend

.PHONY: all clean install links
