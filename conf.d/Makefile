DIR=	${CONFDIR}
CONF=	bootmisc fsck hostname localmount network staticroute urandom

TARGETS+=	network staticroute
CLEANFILES+=	network staticroute

MK=	../mk
include ${MK}/os.mk

ifeq ($(OS),FreeBSD)
CONF+=	ipfw moused powerd rarpd savecore syscons
else ifeq ($(OS),Linux)
CONF+=	consolefont dmesg hwclock keymaps killprocs modules
SOS=	Linux
else ifeq ($(OS),NetBSD)
CONF+=	moused rarpd savecore
endif

include ${MK}/scripts.mk

SOS?=	BSD

network: network.in network.${SOS}
	cp $@.in $@
	[ -e $@.${SOS} ] && cat $@.${SOS} >> $@ || true

staticroute: staticroute.${SOS}
	cp $@.${SOS} $@
