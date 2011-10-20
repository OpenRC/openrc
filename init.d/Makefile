DIR=	${INITDIR}
SRCS=	bootmisc.in fsck.in hostname.in local.in localmount.in netmount.in \
	network.in root.in savecache.in staticroute.in swap.in swclock.in \
	sysctl.in urandom.in ${SRCS-${OS}}
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

NET_LO-FreeBSD=	net.lo0
# Generic BSD scripts
SRCS-FreeBSD=	hostid.in moused.in newsyslog.in pf.in rarpd.in rc-enabled.in \
		rpcbind.in savecore.in syslogd.in
# These are FreeBSD specific
SRCS-FreeBSD+=	adjkerntz.in devd.in dumpon.in ipfw.in mixer.in nscd.in \
		powerd.in syscons.in

NET_LO-Linux=	net.lo
SRCS-Linux=	devfs.in dmesg.in hwclock.in consolefont.in keymaps.in \
		killprocs.in modules.in mount-ro.in mtab.in numlock.in \
		procfs.in sysfs.in termencoding.in

NET_LO-NetBSD=	net.lo0
# Generic BSD scripts
SRCS-NetBSD=	hostid.in moused.in newsyslog.in pf.in rarpd.in rc-enabled.in \
		rpcbind.in savecore.in syslogd.in
# These are NetBSD specific
SRCS-NetBSD+=	devdb.in swap-blk.in ttys.in wscons.in

${SFX}:
	sed ${SED_REPLACE} ${SED_EXTRA} $< > $@

include ${MK}/scripts.mk

_installafter_: realinstall

_installafter_net.lo: realinstall
	${INSTALL} -m ${BINMODE} net.lo ${DESTDIR}/${INITDIR}/${NET_LO}
