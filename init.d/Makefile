include ../mk/net.mk

DIR=	${INITDIR}
SRCS=	bootmisc.in fsck.in hostname.in local.in localmount.in loopback.in \
	netmount.in osclock.in root.in savecache.in swap.in swapfiles.in \
	tmpfiles.setup.in swclock.in sysctl.in urandom.in s6-svscan.in ${SRCS-${OS}}
BIN=	${OBJS}

# Are we installing our network scripts?
ifeq (${MKNET},yes)
SRCS+= network.in staticroute.in
endif

MK=	../mk
include ${MK}/os.mk

# Generic BSD scripts
SRCS-FreeBSD=	hostid.in moused.in newsyslog.in pf.in rarpd.in rc-enabled.in \
		rpcbind.in savecore.in syslogd.in
# These are FreeBSD specific
SRCS-FreeBSD+=	adjkerntz.in devd.in dumpon.in encswap.in ipfw.in \
		mixer.in nscd.in powerd.in syscons.in

SRCS-Linux=	binfmt.in devfs.in dmesg.in hwclock.in consolefont.in keymaps.in \
		killprocs.in modules.in mount-ro.in mtab.in numlock.in \
		procfs.in sysfs.in termencoding.in tmpfiles.dev.in

# Generic BSD scripts
SRCS-NetBSD=	hostid.in moused.in newsyslog.in pf.in rarpd.in rc-enabled.in \
		rpcbind.in savecore.in syslogd.in
# These are NetBSD specific
SRCS-NetBSD+=	devdb.in swap-blk.in ttys.in wscons.in

%.in: %${SFX}
	sed ${SED_REPLACE} ${SED_EXTRA} $< > $@

include ${MK}/scripts.mk

_installafter_: realinstall
