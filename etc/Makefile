DIR=	${SYSCONFDIR}
SRCS=	rc.conf.in rc.in rc.shutdown.in
BIN=	${BIN-${OS}}
CONF=	rc.conf ${BIN-${OS}}

CLEANFILES+=	rc.conf

MK=	../mk
include ${MK}/os.mk

SED_EXTRA-FreeBSD=	-e 's:@TERM@:cons25:g'
BIN-FreeBSD=		rc rc.shutdown rc.devd
CONF-FreeBSD=		devd.conf

SED_EXTRA-Linux=	-e 's:@TERM@:wsvt25:g'
BIN-Linux=
CONF-Linux=

SED_EXTRA-NetBSD=	-e 's:@TERM@:wsvt25:g'
BIN-NetBSD=		rc rc.shutdown
CONF-NetBSD=

SED_EXTRA=	${SED_EXTRA-${OS}}

include ${MK}/scripts.mk

rc.conf: rc.conf.in rc.conf.${OS}
	${SED} ${SED_REPLACE} ${SED_EXTRA} $^ > $@
