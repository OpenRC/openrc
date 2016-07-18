include ../mk/net.mk

DIR=	${CONFDIR}
CONF=	bootmisc fsck hostname localmount netmount urandom tmpfiles \
		${CONF-${OS}}

ifeq (${MKNET},yes)
CONF+= network staticroute
TARGETS+=	staticroute
CLEANFILES+=	staticroute
endif

MK=	../mk
include ${MK}/os.mk

CONF-FreeBSD=	ipfw modules moused powerd rarpd savecore syscons

CONF-Linux=	consolefont devfs dmesg hwclock keymaps killprocs modules mtab \
	net-online

CONF-NetBSD=	moused rarpd savecore

include ${MK}/scripts.mk

staticroute: staticroute${SFX}
	cp $@${SFX} $@
