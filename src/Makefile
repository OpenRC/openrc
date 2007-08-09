# Copyright 1999-2007 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2

CC ?= gcc

CFLAGS ?= -Wall -O2 -pipe

check_gcc=$(shell if $(CC) $(1) -S -o /dev/null -xc /dev/null >/dev/null 2>&1; \
	  then echo "$(1)"; else echo "$(2)"; fi)

# Loads of nice flags to ensure our code is good
CFLAGS += -pedantic -std=c99 \
    -Wall -Wunused -Wimplicit -Wshadow -Wformat=2 \
    -Wmissing-declarations -Wno-missing-prototypes -Wwrite-strings \
    -Wbad-function-cast -Wnested-externs -Wcomment -Winline \
    -Wchar-subscripts -Wcast-align -Wno-format-nonliteral \
    $(call check_gcc, -Wdeclaration-after-statement) \
    $(call check_gcc, -Wsequence-point) \
    $(call check_gcc, -Wextra)

# For debugging. -Werror is pointless due to ISO C issues with dlsym
#CFLAGS += -ggdb

DESTDIR =
LIB = lib

LIBEINFOSOVER = 0
LIBEINFOSO = libeinfo.so.$(LIBRCSOVER)
LIBEINFOOBJS= libeinfo.o

LIBRCSOVER = 0
LIBRCSO = librc.so.$(LIBRCSOVER)
LIBRCOBJS = librc.o librc-depend.o librc-daemon.o librc-misc.o librc-strlist.o

RCOBJS = env-update.o fstabinfo.o mountinfo.o \
		 rc-depend.o rc-plugin.o rc-status.o rc-update.o runscript.o \
		 start-stop-daemon.o

LIB_TARGETS = $(LIBEINFOSO) $(LIBRCSO)
SBIN_TARGETS = rc
SYS_WHITELIST = env_whitelist

TARGET = $(LIB_TARGETS) $(BIN_TARGETS) $(SBIN_TARGETS) $(PRIV_BIN_TARGETS)

RCLINKS = einfon einfo ewarnn ewarn eerrorn eerror ebegin eend ewend \
	      eindent eoutdent eval_ecolors \
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
		  fstabinfo mountinfo
BINLINKS = rc-status
SBINLINKS = env-update rc-update runscript start-stop-daemon

# Quick hack to make my life easier on BSD and Linux
ifeq ($(OS),)
OS=$(shell uname -s)
ifneq ($(OS),Linux)
OS=BSD
endif
endif

ifeq ($(OS),Linux)
LDLIBS_RC = -ldl
LDLIBS_RS = -ldl
# Shouldn't need this, but it's the easiest workaround for silly
# Linux headers that don't work with -std=c99
override CPPFLAGS += -D_GNU_SOURCE
endif
ifeq ($(OS),BSD)
LDLIBS_LIBRC = -lkvm
endif

HAVE_PAM = 
ifdef HAVE_PAM
CPPFLAGS_SSD = -DHAVE_PAM
LDLIBS_SSD += -lpam
endif

# We also define _BSD_SOURCE so both Linux and the BSDs get a few
# handy functions which makes our lives a lot easier
override CPPFLAGS += -DLIB=\"$(LIB)\"

# IMPORTANT!!!
# Remove this when releasing as it's a security risk
# However, this does save us using libtool when we're testing
# NOTE: The toplevel Makefile for baselayout will automatically
# disable then when doing `make dist`
ifneq ($(wildcard .svn),)
override LDFLAGS += -Wl,-rpath .
endif

all: $(TARGET)

$(LIBEINFOOBJS): override CFLAGS += -fPIC
$(LIBEINFOSO): LDLIBS =
$(LIBEINFOSO): $(LIBEINFOOBJS) 
	$(CC) -fPIC -shared -Wl,-soname,$(LIBEINFOSO) -o $(LIBEINFOSO) $(LIBEINFOOBJS)
	ln -sf $(LIBEINFOSO) libeinfo.so

$(LIBRCOBJS): override CFLAGS += -fPIC
$(LIBRCSO): $(LIBRCOBJS) 
	$(CC) -fPIC -shared -Wl,-soname,$(LIBRCSO) -o $(LIBRCSO) $(LDLIBS_LIBRC) $(LIBRCOBJS)
	ln -sf $(LIBRCSO) librc.so

rc: override CPPFLAGS += $(CPPFLAGS_SSD)
rc: override LDLIBS += $(LDLIBS_RC) $(LDLIBS_RS) $(LDLIBS_SSD)
rc: $(LIBEINFOSO) $(LIBRCSO) $(RCOBJS) rc.o

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
	install -m 0755 -d $(DESTDIR)/$(LIB)/rcscripts/conf.d
	install -m 0644 $(SYS_WHITELIST) $(DESTDIR)/$(LIB)/rcscripts/conf.d
	install -m 0755 -d $(DESTDIR)/$(LIB)/rcscripts/bin
	for x in $(BINLINKS); do ln -sf ../sbin/rc $(DESTDIR)/bin/$$x; done
	for x in $(SBINLINKS); do ln -sf rc $(DESTDIR)/sbin/$$x; done
	for x in $(RCLINKS); do ln -sf ../../../sbin/rc $(DESTDIR)/$(LIB)/rcscripts/bin/$$x; done
	if test "$(HAVE_PAM)" != "" ; then \
		install -m 0755 -d $(DESTDIR)/etc/pam.d ; \
		install -m 0644 start-stop-daemon.pam $(DESTDIR)/etc/pam.d/start-stop-daemon ; \
	fi

clean:
	rm -f $(TARGET) $(BINLINKS) $(SBINLINKS) $(RCLINKS) $(RCPRIVLINKS)
	rm -f *.o *~ *.core *.so .depend

-include .depend
.depend: $(wildcard *.c *.h)
	$(CC) $(CPPFLAGS) -MM *.c > .depend

.PHONY: all clean install links
