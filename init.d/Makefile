DIR=	${INITDIR}
SRCS=	bootmisc.in fsck.in hostname.in local.in localmount.in netmount.in \
	network.in root.in savecache.in swap.in sysctl.in urandom.in
BIN=	${OBJS}

INSTALLAFTER=	_installafter
CLEANFILES+=	net.lo
TARGETS+=	net.lo

MK=	../mk
include ${MK}/os.mk
include Makefile.${OS}
include ${MK}/scripts.mk

_installafter: realinstall
	${INSTALL} -m ${BINMODE} net.lo ${DESTDIR}/${INITDIR}/${NET_LO}
