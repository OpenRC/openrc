BOOT=		bootmisc fsck hostname localmount \
		root swap sysctl urandom
DEFAULT=	local netmount

LEVELDIR=	${DESTDIR}/${SYSCONFDIR}/runlevels
SYSINITDIR=	${LEVELDIR}/sysinit
BOOTDIR=	${LEVELDIR}/boot
DEFAULTDIR=	${LEVELDIR}/default

INITDIR=	../init.d

MK=		../mk
include ${MK}/sys.mk
include ${MK}/os.mk
include ${MK}/gitignore.mk
include Makefile.${OS}

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
				test "$$x" = "net.lo" -o "$$x" = "net.lo0" && continue; \
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

check test::

clean:
