DIR=	${INITDIR}
SRCS=	avahi-dnsconfd.in avahid.in dhcpcd.in dbus.in \
	hald.in named.in ntpd.in \
	openvpn.in polkitd.in sshd.in wpa_supplicant.in

MK=	../mk

SED_EXTRA+= -e 's:@VARBASE@:/var:g'

include ${MK}/scripts.mk

ifeq ($(OS),FreeBSD)
PKG_PREFIX?=	/usr/local
else ifeq ($(OS),Linux)
PKG_PREFIX?=/usr
else ifeq ($(OS),NetBSD)
PKG_PREFIX?=	/usr/pkg
endif
