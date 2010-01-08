DIR=	${CONFDIR}
CONF=	bootmisc fsck hostname local localmount network staticroute urandom

TARGETS+=	network staticroute
CLEANFILES+=	network staticroute

MK=	../mk
include ${MK}/os.mk
include Makefile.${OS}
include ${MK}/scripts.mk

SOS?=	BSD

network: network.in network.${SOS}
	cp $@.in $@
	[ -e $@.${SOS} ] && cat $@.${SOS} >> $@ || true

staticroute: staticroute.${SOS}
	cp $@.${SOS} $@
