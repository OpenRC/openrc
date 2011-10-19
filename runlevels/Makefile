BOOT=		bootmisc fsck hostname localmount network \
		root staticroute swap sysctl urandom
DEFAULT=	local netmount
SHUTDOWN=	savecache

LEVELDIR=	${DESTDIR}/${SYSCONFDIR}/runlevels
SYSINITDIR=	${LEVELDIR}/sysinit
BOOTDIR=	${LEVELDIR}/boot
DEFAULTDIR=	${LEVELDIR}/default
SHUTDOWNDIR=	${LEVELDIR}/shutdown

INITDIR=	../init.d

MK=		../mk
include ${MK}/sys.mk
include ${MK}/os.mk
include ${MK}/gitignore.mk

ifeq (${OS},BSD)
BOOT+=		hostid newsyslog savecore syslogd swap-blk
else ifeq (${OS},FreeBSD)
# Generic BSD stuff
BOOT+=		hostid net.lo0 newsyslog savecore syslogd

# FreeBSD specific stuff
BOOT+=		adjkerntz dumpon syscons
else ifeq (${OS},Linux)
SYSINIT+=	devfs dmesg
BOOT+=		hwclock keymaps modules mtab procfs termencoding
SHUTDOWN+=	killprocs mount-ro
else ifeq (${OS},NetBSD)
# Generic BSD stuff
BOOT+=		hostid net.lo0 newsyslog savecore syslogd

# NetBSD specific stuff
BOOT+=		devdb swap-blk ttys wscons
endif

all:

install:
	if ! test -d "${SYSINITDIR}"; then \
		${INSTALL} -d ${SYSINITDIR} || exit $$?; \
		for x in ${SYSINIT}; do \
			if test -n "${PREFIX}"; then \
				grep -q "keyword .*noprefix" ${INITDIR}/"$$x" && continue; \
			fi; \
			ln -snf ${PREFIX}/etc/init.d/"$$x" ${SYSINITDIR}/"$$x" || exit $$?; done \
	fi
	if ! test -d "${BOOTDIR}"; then \
		${INSTALL} -d ${BOOTDIR} || exit $$?; \
		for x in ${BOOT}; do \
			if test -n "${PREFIX}"; then \
				grep -q "keyword .*noprefix" ${INITDIR}/"$$x" && continue; \
			fi; \
			ln -snf ${PREFIX}/etc/init.d/"$$x" ${BOOTDIR}/"$$x" || exit $$?; \
		done \
	fi
	if ! test -d "${DEFAULTDIR}"; then \
		${INSTALL} -d ${DEFAULTDIR} || exit $$?; \
		for x in ${DEFAULT}; do \
			if test -n "${PREFIX}"; then \
				grep -q "keyword .*noprefix" ${INITDIR}/"$$x" && continue; \
			fi; \
			ln -snf ${PREFIX}/etc/init.d/"$$x" ${DEFAULTDIR}/"$$x" || exit $$?; done \
	fi
	if ! test -d "${SHUTDOWNDIR}"; then \
		${INSTALL} -d ${SHUTDOWNDIR} || exit $$?; \
		for x in ${SHUTDOWN}; do \
			if test -n "${PREFIX}"; then \
				grep -q "keyword .*noprefix" ${INITDIR}/"$$x" && continue; \
			fi; \
			ln -snf ${PREFIX}/etc/init.d/"$$x" ${SHUTDOWNDIR}/"$$x" || exit $$?; done \
	fi

check test::

clean:
