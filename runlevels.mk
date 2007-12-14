# Handy runlevel stuff

LEVELDIR = $(DESTDIR)/etc/runlevels
BOOTDIR = $(LEVELDIR)/boot
DEFAULTDIR = $(LEVELDIR)/default

install:
	if ! test -d "$(BOOTDIR)"; then \
		$(INSTALL) -d $(BOOTDIR) || exit $$?; \
		for x in $(BOOT); do ln -snf ../../init.d/"$$x" $(BOOTDIR)/"$$x" || exit $$?; done \
	fi
	if ! test -d "$(DEFAULTDIR)"; then \
		$(INSTALL) -d $(DEFAULTDIR) || exit $$?; \
		for x in $(DEFAULT); do ln -snf ../../init.d/"$$x" $(DEFAULTDIR)/"$$x" || exit $$?; done \
	fi

all:
clean:

# vim: set ts=4 :
