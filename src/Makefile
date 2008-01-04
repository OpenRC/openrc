# Copyright 2007-2008 Roy Marples 

# We should strive to keep this Makefile working with the default make
# shipped with the OS's we support. Mainly because I'm lazy and just want
# to type make instead of gmake, but also so that other distros can pick
# it up and not rely on GNU Make.

CC ?= gcc
AR ?= ar
RANLIB ?= ranlib
CFLAGS += -O2 -pipe
LDFLAGS += -L.

# GNU Make way of detecting gcc flags we can use
check_gcc=$(shell if $(CC) $(1) -S -o /dev/null -xc /dev/null >/dev/null 2>&1; \
	  then echo "$(1)"; else echo "$(2)"; fi)

# pmake check for extra cflags 
WEXTRA != for x in -Wdeclaration-after-statement -Wsequence-point -Wextra; do \
	if $(CC) -W$$x -S -o /dev/null -xc /dev/null >/dev/null 2>&1; \
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

# Set PAM = pam for pam support
PAM = 

# Set TERMCAP = termcap for termcap support
TERMCAP =

LIBEINFOSOVER = 1
LIBEINFOSO = libeinfo.so.$(LIBRCSOVER)
LIBEINFOOBJS= libeinfo.o
LDLIBS_LIBEINFO =

LIBRCSOVER = 1
LIBRCSO = librc.so.$(LIBRCSOVER)
LIBRCOBJS = librc.o librc-depend.o librc-daemon.o librc-misc.o librc-strlist.o
LDLIBS_LIBRC =

RCOBJS = checkpath.o fstabinfo.o mountinfo.o \
		 rc-depend.o rc-logger.o rc-misc.o rc-plugin.o rc-status.o \
		 rc-update.o \
		 runscript.o  start-stop-daemon.o rc.o
LDLIBS_RC = -leinfo -lrc -lutil

LIB_TARGETS = $(LIBEINFOSO) $(LIBRCSO)
ULIB_TARGETS = libeinfo.a librc.a
SBIN_TARGETS = rc
SYS_WHITELIST = env_whitelist

TARGET = $(LIB_TARGETS) $(BIN_TARGETS) $(SBIN_TARGETS)

RC_BINLINKS = einfon einfo ewarnn ewarn eerrorn eerror ebegin eend ewend \
			  eindent eoutdent esyslog eval_ecolors \
			  veinfo vewarn vebegin veend vewend veindent veoutdent \
			  service_starting service_started \
			  service_stopping service_stopped \
			  service_inactive service_wasinactive \
			  service_coldplugged service_started_daemon \
			  checkpath fstabinfo mountinfo rc-depend \
			  service_get_value service_set_value get_options save_options \
			  shell_var
RC_SBINLINKS =  mark_service_starting mark_service_started \
				mark_service_stopping mark_service_stopped \
				mark_service_inactive mark_service_wasinactive \
				mark_service_coldplugged mark_service_failed \
				rc-abort
BINLINKS = rc-status
SBINLINKS = rc-update runscript start-stop-daemon
ALL_LINKS = $(BINLINKS) $(SBINLINKS) $(RC_BINLINKS) $(RC_SBINLINKS)

CPPFLAGS += -DLIB=\"$(LIB)\"

# IMPORTANT!!!
# Ensure that we don't ship with a .svn directory to avoid RPATH security
# issues. However, this does ease development a little
LDFLAGS += -Wl,-rpath .

# Load an optional Makefiles - the order is important
TOPDIR = ..
include $(TOPDIR)/default.mk
include Makefile.$(TERMCAP)
include Makefile.$(OS)
include Makefile.$(PAM)

all: .depend $(TARGET)

version.h:
	sed -n -e 's/^VERSION =[[:space:]]*\([^[:space:]]*\).*/#define VERSION "\1\"/p' ../Makefile > version.h
	if test -n "${BRANDING}"; then \
		echo "#define BRANDING \"${BRANDING}\"" >> version.h; \
	fi

$(LIBEINFOOBJS):
	$(CC) $(CPPFLAGS) $(CPPFLAGS_LIBEINFO) $(CFLAGS) -fPIC -c $<
$(LIBEINFOSO): einfo.map $(LIBEINFOOBJS) 
	$(CC) $(LDFLAGS) -fPIC -shared \
		-Wl,-soname,$(LIBEINFOSO) \
		-Wl,-version-script einfo.map \
		-o $(LIBEINFOSO) $(LIBEINFOOBJS) $(LDLIBS_LIBEINFO)
	ln -sf $(LIBEINFOSO) libeinfo.so
	$(AR) rc libeinfo.a $(LIBEINFOOBJS)
	$(RANLIB) libeinfo.a

$(LIBRCOBJS):
	$(CC) $(CPPFLAGS) $(CFLAGS) -fPIC -c $<
$(LIBRCSO): rc.map $(LIBRCOBJS)
	$(CC) $(LDFLAGS) -fPIC -shared \
		-Wl,-soname,$(LIBRCSO) \
		-Wl,-version-script rc.map \
		-o $(LIBRCSO) $(LIBRCOBJS) $(LDLIBS_LIBRC)
	ln -sf $(LIBRCSO) librc.so
	$(AR) rc librc.a $(LIBRCOBJS)
	$(RANLIB) librc.a

$(RCOBJS):
	$(CC) $(CPPFLAGS) $(CPPFLAGS_SSD) $(CFLAGS) -c $<
rc: version.h $(LIBEINFOSO) $(LIBRCSO) $(RCOBJS)
	$(CC) $(LDFLAGS) $(BIN_LDFLAGS) -o rc $(RCOBJS) $(LDLIBS_RC)

$(ALL_LINKS): rc
	ln -sf rc $@
links: $(ALL_LINKS)

install:: $(TARGET)
	install -d $(DESTDIR)/$(LIB)
	install -m 0444 $(LIB_TARGETS) $(DESTDIR)/$(LIB)
	install -d $(DESTDIR)/usr/$(LIB)
	install -m 0444 $(ULIB_TARGETS) $(DESTDIR)/usr/$(LIB)
	ln -sf /$(LIB)/$(LIBEINFOSO) $(DESTDIR)/usr/$(LIB)/libeinfo.so
	ln -sf /$(LIB)/$(LIBRCSO) $(DESTDIR)/usr/$(LIB)/librc.so
	install -d $(DESTDIR)/usr/include
	install -m 0444 einfo.h rc.h $(DESTDIR)/usr/include
	install -d $(DESTDIR)/bin
	install -d $(DESTDIR)/sbin
	install -m 0555 $(SBIN_TARGETS) $(DESTDIR)/sbin
	install -d $(DESTDIR)/$(RC_LIB)/conf.d
	install -m 0444 $(SYS_WHITELIST) $(DESTDIR)/$(RC_LIB)/conf.d
	install -d $(DESTDIR)/$(RC_LIB)/bin
	install -d $(DESTDIR)/$(RC_LIB)/sbin
	for x in $(BINLINKS); do ln -sf /sbin/rc $(DESTDIR)/bin/$$x; done
	for x in $(SBINLINKS); do ln -sf rc $(DESTDIR)/sbin/$$x; done
	for x in $(RC_BINLINKS); do ln -sf /sbin/rc $(DESTDIR)/$(RC_LIB)/bin/$$x; done
	for x in $(RC_SBINLINKS); do ln -sf /sbin/rc $(DESTDIR)/$(RC_LIB)/sbin/$$x; done
	if test "$(PAM)" = "pam"; then \
		install -d $(DESTDIR)/etc/pam.d; \
		install -m 0644 start-stop-daemon.pam $(DESTDIR)/etc/pam.d/start-stop-daemon; \
	fi

clean-links:
	rm -f $(ALL_LINKS)
clean:: clean-links
	echo > .depend
	touch -r Makefile .depend
	rm -f $(TARGET) version.h
	rm -f *.o *~ *.core *.so *.a

check:
	$(MAKE) -C test $@

include .depend
_DEPS != ls *.c *.h
.depend: version.h $(_DEPS)$(wildcard *.c *.h)
	$(CC) $(CPPFLAGS) -MM *.c > .depend

depend::
	$(CC) $(CPPFLAGS) -MM *.c > .depend
