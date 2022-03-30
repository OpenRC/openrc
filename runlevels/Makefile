include ../mk/net.mk

BOOT=		bootmisc fsck hostname localmount loopback \
		root swap sysctl ${BOOT-${OS}}
DEFAULT=	local netmount
NONETWORK=	local
SHUTDOWN=	savecache ${SHUTDOWN-${OS}}
SYSINIT=	${SYSINIT-${OS}}

LEVELDIR=	${DESTDIR}/${SYSCONFDIR}/runlevels
SYSINITDIR=	${LEVELDIR}/sysinit
BOOTDIR=	${LEVELDIR}/boot
DEFAULTDIR=	${LEVELDIR}/default
NONETWORKDIR=	${LEVELDIR}/nonetwork
SHUTDOWNDIR=	${LEVELDIR}/shutdown

ifeq (${MKNET},yes)
BOOT+= network staticroute
endif

INITFILES=	../init.d

MK=		../mk
include ${MK}/sys.mk
include ${MK}/os.mk
include ${MK}/gitignore.mk

BOOT-${OS}=
SHUTDOWN-${OS}=
SYSINIT-${OS}=

BOOT-BSD=	hostid newsyslog savecore syslogd swap-blk urandom

# Generic BSD stuff
BOOT-FreeBSD+=	hostid modules newsyslog savecore syslogd urandom
# FreeBSD specific stuff
BOOT-FreeBSD+=	adjkerntz dumpon syscons

BOOT-Linux+=	binfmt hwclock keymaps modules mtab procfs save-keymaps \
	save-termencoding seedrng termencoding
SHUTDOWN-Linux=	killprocs mount-ro
SYSINIT-Linux=	devfs cgroups dmesg sysfs

# Generic BSD stuff
BOOT-NetBSD+=	hostid newsyslog savecore syslogd urandom
# NetBSD specific stuff
BOOT-NetBSD+=	devdb swap-blk ttys wscons

all:

install:
	if ! test -d "${SYSINITDIR}"; then \
		${INSTALL} -d ${SYSINITDIR} || exit $$?; \
		for x in ${SYSINIT}; do \
			if test "${MKPREFIX}" = yes; then \
				grep -q "keyword .*-prefix" ${INITFILES}/"$$x" && continue; \
			fi; \
			ln -snf ${INITDIR}/"$$x" ${SYSINITDIR}/"$$x" || exit $$?; done \
	fi
	if ! test -d "${BOOTDIR}"; then \
		${INSTALL} -d ${BOOTDIR} || exit $$?; \
		for x in ${BOOT}; do \
			if test "${MKPREFIX}" = yes; then \
				grep -q "keyword .*-prefix" ${INITFILES}/"$$x" && continue; \
			fi; \
			ln -snf ${INITDIR}/"$$x" ${BOOTDIR}/"$$x" || exit $$?; \
		done \
	fi
	if ! test -d "${DEFAULTDIR}"; then \
		${INSTALL} -d ${DEFAULTDIR} || exit $$?; \
		for x in ${DEFAULT}; do \
			if test "${MKPREFIX}" = yes; then \
				grep -q "keyword .*-prefix" ${INITFILES}/"$$x" && continue; \
			fi; \
			ln -snf ${INITDIR}/"$$x" ${DEFAULTDIR}/"$$x" || exit $$?; done \
	fi
	if ! test -d "${NONETWORKDIR}"; then \
		${INSTALL} -d ${NONETWORKDIR} || exit $$?; \
		for x in ${NONETWORK}; do \
			if test "${MKPREFIX}" = yes; then \
				grep -q "keyword .*-prefix" ${INITFILES}/"$$x" && continue; \
			fi; \
			ln -snf ${INITDIR}/"$$x" ${NONETWORKDIR}/"$$x" || exit $$?; done \
	fi
	if ! test -d "${SHUTDOWNDIR}"; then \
		${INSTALL} -d ${SHUTDOWNDIR} || exit $$?; \
		for x in ${SHUTDOWN}; do \
			if test "${MKPREFIX}" = yes; then \
				grep -q "keyword .*-prefix" ${INITFILES}/"$$x" && continue; \
			fi; \
			ln -snf ${INITDIR}/"$$x" ${SHUTDOWNDIR}/"$$x" || exit $$?; done \
	fi
	if test "${MKSYSVINIT}" = yes && test "${OS}" = Linux; then \
		for x in tty1 tty2 tty3 tty4 tty5 tty6; do \
			ln -snf ${INITDIR}/agetty ${DESTDIR}/${INITDIR}/"agetty.$$x" || exit $$?; \
			ln -snf ${INITDIR}/agetty.$$x ${DEFAULTDIR}/"agetty.$$x" || exit $$?; \
		done; \
	fi

check test::

clean:
