BOOT=		bootmisc fsck hostname localmount \
		root swap sysctl urandom ${BOOT-${OS}}
DEFAULT=	local netmount
SHUTDOWN=	savecache ${SHUTDOWN-${OS}}
SYSINIT=	${SYSINIT-${OS}}

LEVELDIR=	${DESTDIR}/${SYSCONFDIR}/runlevels
SYSINITDIR=	${LEVELDIR}/sysinit
BOOTDIR=	${LEVELDIR}/boot
DEFAULTDIR=	${LEVELDIR}/default
SHUTDOWNDIR=	${LEVELDIR}/shutdown

MK=		../mk
include ${MK}/sys.mk
include ${MK}/os.mk
include ${MK}/gitignore.mk

BOOT-${OS}=
SHUTDOWN-${OS}=
SYSINIT-${OS}=

BOOT-BSD=	hostid newsyslog savecore syslogd swap-blk

# Generic BSD stuff
BOOT-FreeBSD=	hostid net.lo0 newsyslog savecore syslogd
# FreeBSD specific stuff
BOOT-FreeBSD+=	adjkerntz dumpon syscons

BOOT-Linux=	hwclock keymaps modules mtab net.lo procfs termencoding
SHUTDOWN-Linux=	killprocs mount-ro
SYSINIT-Linux=	devfs dmesg

# Generic BSD stuff
BOOT-NetBSD=	hostid net.lo0 newsyslog savecore syslogd
# NetBSD specific stuff
BOOT-NetBSD+=	devdb swap-blk ttys wscons

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
