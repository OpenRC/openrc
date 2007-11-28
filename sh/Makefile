DIR = $(RC_LIB)/sh
INC = functions.sh init-functions.sh init-common-post.sh \
	  rc-functions.sh rc-mount.sh
BIN = gendepends.sh net.sh rc-mount.sh rc-help.sh runscript.sh

TOPDIR = ..
include $(TOPDIR)/default.mk

install::
	# Handle lib correctly
	if test $(LIB) != "lib"; then \
		sed -i'.bak' -e 's,/lib/,/$(LIB)/,g' $(DESTDIR)/$(RC_LIB)/sh/functions.sh || exit $$?; \
		rm -f $(DESTDIR)/$(RC_LIB)/sh/functions.sh.bak; \
		sed -i'.bak' -e 's,/lib/,/$(LIB)/,g' $(DESTDIR)/$(RC_LIB)/sh/rc-functions.sh || exit $$?; \
		rm -f $(DESTDIR)/$(RC_LIB)/sh/rc-functions.sh.bak; \
	fi
