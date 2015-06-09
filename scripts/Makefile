MK=	../mk
include ${MK}/os.mk

DIR=	${LIBEXECDIR}/bin
BIN=	on_ac_power
INSTALLAFTER = _installafter

ifeq (${OS},Linux)
SRCS+= rc-sstat.in
BIN+=	rc-sstat
endif

_installafter:
ifeq (${OS},Linux)
	${INSTALL} -d ${DESTDIR}${SBINDIR}
	ln -s ${DIR}/rc-sstat ${DESTDIR}/${SBINDIR}/rc-sstat
endif

include ${MK}/scripts.mk
