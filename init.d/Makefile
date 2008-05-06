DIR=	${INITDIR}
SRCS=	bootmisc.in fsck.in halt.sh.in hostname.in local.in localmount.in \
	netmount.in root.in swap.in sysctl.in urandom.in
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
