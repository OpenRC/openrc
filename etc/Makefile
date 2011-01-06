DIR=	${SYSCONFDIR}
CONF=	rc.conf

CLEANFILES+=	rc.conf

MK=	../mk
include ${MK}/os.mk
include Makefile.${OS}
include ${MK}/scripts.mk

rc.conf: rc.conf.in rc.conf.${OS}
	${SED} ${SED_REPLACE} ${SED_EXTRA} $^ > $@
