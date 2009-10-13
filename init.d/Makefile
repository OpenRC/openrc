DIR=	${INITDIR}
SRCS=	bootmisc.in fsck.in hostname.in local.in localmount.in netmount.in \
	network.in root.in savecache.in swap.in swclock.in sysctl.in urandom.in
BIN=	${OBJS}

# Build our old net foo or not
_OLDNET_SH=	case "${MKOLDNET}" in \
		[Yy][Ee][Ss]) echo "net.lo";; \
		*) echo "";; \
		esac
_OLDNET!=	${_OLDNET_SH}
_NET_LO=	${_OLDNET}$(shell ${_OLDNET_SH})

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
