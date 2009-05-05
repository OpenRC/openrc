DIR=	${CONFDIR}
CONF=	bootmisc fsck hostname local localmount network urandom

TARGETS+=	network
CLEANFILES+=	network

MK=	../mk
include ${MK}/os.mk
include Makefile.${OS}
include ${MK}/scripts.mk

network: network.in network.${OS}
	cp network.in network
	[ -e network.${OS} ] && cat network.${OS} >> network || true
