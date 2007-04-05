BOOT = consolefont keymaps modules net.lo volumes
DEFAULT = hdparm

install:
	install -d -m 0755 $(DESTDIR)/etc/runlevels/boot || exit $$? ; \
	for x in $(BOOT) ; do ln -snf ../../init.d/"$$x" $(DESTDIR)/etc/runlevels/boot/"$$x" || exit $$? ; done ; \
	install -d -m 0755 $(DESTDIR)/etc/runlevels/default || exit $$? ; \
	for x in $(DEFAULT) ; do ln -snf ../../init.d/"$$x" $(DESTDIR)/etc/runlevels/default/"$$x" || exit $$? ; done ; \

.PHONY: all clean

# vim: set ts=4 :
