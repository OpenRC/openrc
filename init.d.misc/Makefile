DIR=	${INITDIR}
SRCS=	avahi-dnsconfd.in avahid.in dbus.in hald.in named.in ntpd.in \
	openvpn.in polkitd.in sshd.in

MK=	../mk

SED_EXTRA+= -e 's:@VARBASE@:/var:g'

include ${MK}/scripts.mk
include Makefile.${OS}
