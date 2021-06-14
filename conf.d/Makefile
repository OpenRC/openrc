include ../mk/net.mk

DIR=	${CONFDIR}
CONF=	bootmisc fsck hostname localmount netmount swap urandom ${CONF-${OS}}

ifeq (${MKNET},yes)
CONF+= network staticroute
endif

MK=	../mk
include ${MK}/os.mk

CONF-FreeBSD=	ipfw modules modules-late moused powerd rarpd savecore \
	syscons

CONF-Linux=	agetty consolefont devfs dmesg hwclock keymaps killprocs modules \
	modules-late mtab net-online

CONF-NetBSD=	moused rarpd savecore

include ${MK}/scripts.mk
