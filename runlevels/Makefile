BOOT=		bootmisc checkroot checkfs hostname localmount \
		swap sysctl urandom
DEFAULT=	local netmount

LEVELDIR=	${DESTDIR}/etc/runlevels
BOOTDIR=	${LEVELDIR}/boot
DEFAULTDIR=	${LEVELDIR}/default

MK=		../mk
include ${MK}/os.mk
include Makefile.${OS}
include Makefile.${SUBOS}

include ${MK}/runlevels.mk
