DIR=	${CONFDIR}
CONF=	bootmisc fsck hostname local localmount network staticroute urandom

TARGETS+=	network staticroute
CLEANFILES+=	network staticroute

MK=	../mk
include ${MK}/os.mk
include Makefile.${OS}
include ${MK}/scripts.mk

SOS?=	BSD

network: network.in network.${OS}
	cp $@.in $@
	[ -e $@.${OS} ] && cat $@.${OS} >> $@ || true

staticroute: staticroute.${SOS}
	cp $@.${SOS} $@
