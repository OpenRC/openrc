BOOT=		bootmisc fsck hostname localmount \
		root swap swapfiles sysctl urandom ${BOOT-${OS}}
DEFAULT=	local netmount
SHUTDOWN=	savecache ${SHUTDOWN-${OS}}
SYSINIT=	${SYSINIT-${OS}}

LEVELDIR=	${DESTDIR}/${SYSCONFDIR}/runlevels
SYSINITDIR=	${LEVELDIR}/sysinit
BOOTDIR=	${LEVELDIR}/boot
DEFAULTDIR=	${LEVELDIR}/default
SHUTDOWNDIR=	${LEVELDIR}/shutdown

ifeq (${MKNET},)
BOOT+= network staticroute
endif

INITDIR=	../init.d

MK=		../mk
include ${MK}/sys.mk
include ${MK}/os.mk
include ${MK}/gitignore.mk

BOOT-${OS}=
SHUTDOWN-${OS}=
SYSINIT-${OS}=

ifeq (${MKNET},oldnet)
BOOT-FreeBSD+= net.lo0
BOOT-Linux+= net.lo
BOOT-NetBSD+= net.lo0
endif

BOOT-BSD=	hostid newsyslog savecore syslogd swap-blk

# Generic BSD stuff
BOOT-FreeBSD+=	hostid newsyslog savecore syslogd
# FreeBSD specific stuff
BOOT-FreeBSD+=	adjkerntz dumpon syscons

BOOT-Linux+=	hwclock keymaps modules mtab procfs termencoding
SHUTDOWN-Linux=	killprocs mount-ro
SYSINIT-Linux=	devfs dmesg

# Generic BSD stuff
BOOT-NetBSD+=	hostid newsyslog savecore syslogd
# NetBSD specific stuff
BOOT-NetBSD+=	devdb swap-blk ttys wscons

all:

install:
	if ! test -d "${SYSINITDIR}"; then \
		${INSTALL} -d ${SYSINITDIR} || exit $$?; \
		for x in ${SYSINIT}; do \
			ln -snf ${PREFIX}/etc/init.d/"$$x" ${SYSINITDIR}/"$$x" || exit $$?; \
		done \
	fi
	if ! test -d "${BOOTDIR}"; then \
		${INSTALL} -d ${BOOTDIR} || exit $$?; \
		for x in ${BOOT}; do \
			ln -snf ${PREFIX}/etc/init.d/"$$x" ${BOOTDIR}/"$$x" || exit $$?; \
		done \
	fi
	if ! test -d "${DEFAULTDIR}"; then \
		${INSTALL} -d ${DEFAULTDIR} || exit $$?; \
		for x in ${DEFAULT}; do \
			ln -snf ${PREFIX}/etc/init.d/"$$x" ${DEFAULTDIR}/"$$x" || exit $$?; \
		done \
	fi
	if ! test -d "${SHUTDOWNDIR}"; then \
		${INSTALL} -d ${SHUTDOWNDIR} || exit $$?; \
		for x in ${SHUTDOWN}; do \
			ln -snf ${PREFIX}/etc/init.d/"$$x" ${SHUTDOWNDIR}/"$$x" || exit $$?; done \
	fi

check test::

clean:
