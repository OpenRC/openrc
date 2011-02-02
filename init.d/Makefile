DIR=	${INITDIR}
SRCS=	bootmisc.in fsck.in hostname.in local.in localmount.in netmount.in \
	network.in root.in savecache.in staticroute.in swap.in swclock.in \
	sysctl.in urandom.in
BIN=	${OBJS}

# Build our old net foo or not
ifeq (${MKOLDNET},yes)
_OLDNET=	net.lo
endif
_NET_LO=	${_OLDNET}

INSTALLAFTER=	_installafter_${_NET_LO}
CLEANFILES+=	${_NET_LO}
TARGETS+=	${_NET_LO}

MK=	../mk
include ${MK}/os.mk
include Makefile.${OS}
include ${MK}/scripts.mk

_installafter_: realinstall

_installafter_net.lo: realinstall
	${INSTALL} -m ${BINMODE} net.lo ${DESTDIR}/${INITDIR}/${NET_LO}
