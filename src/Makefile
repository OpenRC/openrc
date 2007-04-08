# Copyright 1999-2007 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2

CC ?= gcc

CFLAGS ?= -Wall -O2 -pipe
CFLAGS += -pedantic -std=c99 \
    -Wall -Wunused -Wimplicit -Wshadow -Wformat=2 \
    -Wmissing-declarations -Wno-missing-prototypes -Wwrite-strings \
    -Wbad-function-cast -Wnested-externs -Wcomment -Winline \
    -Wchar-subscripts -Wcast-align -Wno-format-nonliteral

# Early GCC versions don't support these flags, so you may need to comment
# this line out
CFLAGS += -Wsequence-point -Wextra -Wdeclaration-after-statement

# For debugging. -Werror is pointless due to ISO C issues with dlsym
#CFLAGS += -ggdb

DESTDIR =
LIB = lib

LIBEINFOSOVER = 0
LIBEINFOSO = libeinfo.so.$(LIBRCSOVER)
LIBEINFOOBJS= libeinfo.o

LIBRCSOVER = 0
LIBRCSO = librc.so.$(LIBRCSOVER)
LIBRCOBJS= librc.o librc-depend.o librc-daemon.o librc-misc.o librc-strlist.o

LIB_TARGETS = $(LIBEINFOSO) $(LIBRCSO)
BIN_TARGETS = rc-status
SBIN_TARGETS = env-update rc rc-update runscript start-stop-daemon
PRIV_BIN_TARGETS = fstabinfo mountinfo rc-depend
SYS_WHITELIST = env_whitelist

TARGET = $(LIB_TARGETS) $(BIN_TARGETS) $(SBIN_TARGETS) $(PRIV_BIN_TARGETS)

RCLINKS = einfon einfo ewarnn ewarn eerrorn eerror ebegin eend ewend \
	      eindent eoutdent eflush color_terminal \
	      veinfo vewarn vebegin veend vewend veindent veoutdent \
	      service_starting service_inactive service_started \
	      service_stopping service_stopped \
	      service_inactive service_wasinactive \
	      service_coldplugged \
	      mark_service_starting mark_service_inactive mark_service_started \
	      mark_service_stopping mark_service_stopped \
	      mark_service_inactive mark_service_wasinactive \
	      mark_service_coldplugged \
	      get_options save_options \
	      is_runlevel_start is_runlevel_stop service_started_daemon

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
override CFLAGS += -D_GNU_SOURCE
endif
ifeq ($(OS),BSD)
override LDLIBS += -lkvm
endif

HAVE_PAM = 
ifdef HAVE_PAM
CFLAGS_SSD = -DHAVE_PAM
LDLIBS_SSD = -lpam
endif

# We also define _BSD_SOURCE so both Linux and the BSDs get a few
# handy functions which makes our lives a lot easier
override CFLAGS += -DLIB=\"$(LIB)\"

# IMPORTANT!!!
# Remove this when releasing as it's a security risk
# However, this does save us using libtool when we're testing
# NOTE: The toplevel Makefile for baselayout will automatically
# disable then when doing `make dist`
##override LDFLAGS += -Wl,-rpath .

all: $(TARGET)

$(LIBEINFOOBJS): CFLAGS += -fPIC
$(LIBEINFOSO): LDLIBS =
$(LIBEINFOSO): $(LIBEINFOOBJS) 
	$(CC) -fPIC -shared -Wl,-soname,$(LIBEINFOSO) -o $(LIBEINFOSO) $(LIBEINFOOBJS)
	ln -sf $(LIBEINFOSO) libeinfo.so

$(LIBRCOBJS): CFLAGS += -fPIC
$(LIBRCSO): $(LIBRCOBJS) 
	$(CC) -fPIC -shared -Wl,-soname,$(LIBRCSO) -o $(LIBRCSO) $(LIBRCOBJS)
	ln -sf $(LIBRCSO) librc.so

splash: CFLAGS += -fPIC
splash: splash.o
	$(CC) -fPIC -shared -Wl,-soname,splash.so -o splash.so splash.o

env-update: $(LIBEINFOSO) $(LIBRCSO) env-update.o

fstabinfo: $(LIBEINFOSO) fstabinfo.o

mountinfo: $(LIBEINFOSO) $(LIBRCSO) mountinfo.o

rc-depend: $(LIBEINFOSO) $(LIBRCSO) rc-depend.o

rc-status: $(LIBEINFOSO) $(LIBRCSO) rc-status.o

rc-update: $(LIBEINFOSO) $(LIBRCSO) rc-update.o

rc: LDLIBS += $(LDLIBS_RC)
rc: $(LIBEINFOSO) $(LIBRCSO) rc-plugin.o rc.o

runscript: LDLIBS += $(LDLIBS_RS)
runscript: $(LIBEINFOSO) $(LIBRCSO) rc-plugin.o runscript.o

start-stop-daemon: CFLAGS += $(CFLAGS_SSD)
start-stop-daemon: LDLIBS += $(LDLIBS_SSD)
start-stop-daemon: $(LIBEINFOSO) $(LIBRCSO) start-stop-daemon.o

links: rc
	for x in $(RCLINKS) $(RCPRIVLINKS); do ln -sf rc $$x; done

install: $(TARGET)
	install -m 0755 -d $(DESTDIR)/$(LIB)
	install -m 0755 $(LIB_TARGETS) $(DESTDIR)/$(LIB)
	ln -sf $(LIBEINFOSO) $(DESTDIR)/$(LIB)/libeinfo.so
	ln -sf $(LIBRCSO) $(DESTDIR)/$(LIB)/librc.so
	install -m 0755 -d $(DESTDIR)/usr/include
	install -m 0644 einfo.h rc.h $(DESTDIR)/usr/include
	install -m 0755 -d $(DESTDIR)/bin
	install -m 0755 $(BIN_TARGETS) $(DESTDIR)/bin
	install -m 0755 -d $(DESTDIR)/sbin
	install -m 0755 $(SBIN_TARGETS) $(DESTDIR)/sbin
	install -m 0755 -d $(DESTDIR)/$(LIB)/rcscripts/conf.d
	install -m 0644 $(SYS_WHITELIST) $(DESTDIR)/$(LIB)/rcscripts/conf.d
	install -m 0755 -d $(DESTDIR)/$(LIB)/rcscripts/bin
	install -m 0755 $(PRIV_BIN_TARGETS) $(DESTDIR)/$(LIB)/rcscripts/bin
	for x in $(RCLINKS); do ln -sf $(DESTDIR)/sbin/rc $(DESTDIR)/$(LIB)/rcscripts/bin/$$x; done
	if test "$(HAVE_PAM)" != "" ; then \
		install -m 0755 -d $(DESTDIR)/etc/pam.d ; \
		install -m 0644 start-stop-daemon.pam $(DESTDIR)/etc/pam.d/start-stop-daemon ; \
	fi

clean:
	rm -f $(TARGET) $(RCLINKS) $(RCPRIVLINKS)
	rm -f *.o *~ *.core *.so
