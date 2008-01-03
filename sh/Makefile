DIR = $(RC_LIB)/sh
INC = functions.sh init-common-post.sh rc-functions.sh rc-mount.sh
BIN = gendepends.sh net.sh runscript.sh

TOPDIR = ..
include $(TOPDIR)/default.mk

# To get NET_LO
include Makefile.$(OS)

install::
	@# Handle lib correctly
	if test $(LIB) != "lib"; then \
		sed -i'.bak' -e 's,/lib/,/$(LIB)/,g' $(DESTDIR)/$(RC_LIB)/sh/functions.sh || exit $$?; \
		rm -f $(DESTDIR)/$(RC_LIB)/sh/functions.sh.bak; \
		sed -i'.bak' -e 's,/lib/,/$(LIB)/,g' $(DESTDIR)/$(RC_LIB)/sh/rc-functions.sh || exit $$?; \
		rm -f $(DESTDIR)/$(RC_LIB)/sh/rc-functions.sh.bak; \
	fi
	@# Provide an init script for the loopback interface
	ln -snf $(RC_LIB)/sh/net.sh $(DESTDIR)/etc/init.d/$(NET_LO) || exit $$?
	@# Put functions.sh into the init.d dir so 3rd part apps don't have to
	@# be multilib aware
	ln -snf $(RC_LIB)/sh/functions.sh $(DESTDIR)/etc/init.d || exit $$?
