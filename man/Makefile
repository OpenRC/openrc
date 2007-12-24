DIR = /usr/share/man
MAN3 = einfo.3
MAN8 = start-stop-daemon.8 rc.8 rc-update.8 runscript.8 

TOPDIR = ..
include $(TOPDIR)/default.mk

install::
	$(INSTALL) -d $(DESTDIR)/$(DIR)/man3
	for man in $(MAN3); do \
		$(INSTALL) -m 0644 "$$man" $(DESTDIR)/$(DIR)/man3 || exit $$?; \
	done	
	$(INSTALL) -d $(DESTDIR)/$(DIR)/man8
	for man in $(MAN8); do \
		$(INSTALL) -m 0644 "$$man" $(DESTDIR)/$(DIR)/man8 || exit $$?; \
	done	
