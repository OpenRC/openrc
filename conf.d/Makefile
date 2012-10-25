DIR=	${CONFDIR}
CONF=	bootmisc fsck hostname localmount netmount urandom tmpfilesd \
		${CONF-${OS}}

ifeq (${MKNET},)
CONF+= network staticroute
TARGETS+=	network staticroute
CLEANFILES+=	network staticroute
endif

MK=	../mk
include ${MK}/os.mk

CONF-FreeBSD=	ipfw moused powerd rarpd savecore syscons

CONF-Linux=	consolefont dmesg hwclock keymaps killprocs modules

CONF-NetBSD=	moused rarpd savecore

include ${MK}/scripts.mk

network: network.in network${SFX}
	cat $^ > $@

staticroute: staticroute${SFX}
	cp $@${SFX} $@
