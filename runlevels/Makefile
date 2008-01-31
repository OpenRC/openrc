BOOT=		bootmisc fsck hostname localmount \
		root swap sysctl urandom
DEFAULT=	local netmount

LEVELDIR=	${DESTDIR}/etc/runlevels
BOOTDIR=	${LEVELDIR}/boot
DEFAULTDIR=	${LEVELDIR}/default

MK=		../mk
include ${MK}/sys.mk
include ${MK}/os.mk
include Makefile.${OS}
include Makefile.${SUBOS}

all:

install:
	if ! test -d "${BOOTDIR}"; then \
		${INSTALL} -d ${BOOTDIR} || exit $$?; \
		for x in ${BOOT}; do ln -snf /etc/init.d/"$$x" ${BOOTDIR}/"$$x" || exit $$?; done \
	fi
	if ! test -d "${DEFAULTDIR}"; then \
		${INSTALL} -d ${DEFAULTDIR} || exit $$?; \
		for x in ${DEFAULT}; do ln -snf /etc/init.d/"$$x" ${DEFAULTDIR}/"$$x" || exit $$?; done \
	fi

clean:
