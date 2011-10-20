DIR=	${CONFDIR}
CONF=	bootmisc fsck hostname localmount network staticroute urandom \
	${CONF-${OS}}

TARGETS+=	network staticroute
CLEANFILES+=	network staticroute

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
