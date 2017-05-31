MK=	../mk
include ${MK}/os.mk

DIR=	${LIBEXECDIR}/bin
BIN=	on_ac_power
INSTALLAFTER = _installafter

ifeq (${OS},Linux)
SRCS+= rc-sstat.in
BIN+=	rc-sstat
ifeq (${MKSYSVINIT},yes)
SRCS+= halt.in poweroff.in reboot.in shutdown.in
BIN+=	halt poweroff reboot shutdown
	endif
endif

_installafter:
ifeq (${OS},Linux)
	${INSTALL} -d ${DESTDIR}${SBINDIR}
	ln -sf ../${DIR}/rc-sstat ${DESTDIR}/${SBINDIR}/rc-sstat
ifeq (${MKSYSVINIT},yes)
	ln -sf	../${DIR}/halt ${DESTDIR}/sbin/halt
	ln -sf	../${DIR}/poweroff ${DESTDIR}/sbin/poweroff
	ln -sf	../${DIR}/reboot ${DESTDIR}/sbin/reboot
	ln -sf	../${DIR}/shutdown ${DESTDIR}/sbin/shutdown
	ln -sf	openrc-init ${DESTDIR}/sbin/init
endif
endif

include ${MK}/scripts.mk
