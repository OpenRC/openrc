DIR=	${LIBEXECDIR}/net
SRCS=	ifconfig.sh.in
INC=	dhclient.sh dhcpcd.sh ifconfig.sh macchanger.sh macnet.sh \
	ssidnet.sh system.sh wpa_supplicant.sh

MK=	../mk
include ${MK}/os.mk

ifeq ($(OS),FreeBSD)
SRCS+=		iwconfig.sh.in
INC+=		iwconfig.sh

.SUFFIXES:	.sh.BSD.in
.sh.BSD.in.sh:
	${CP} $< $@
else ifeq ($(OS),Linux)
SRCS+=		iwconfig.sh.in
INC+=		adsl.sh apipa.sh arping.sh bonding.sh br2684ctl.sh bridge.sh \
		ccwgroup.sh clip.sh ethtool.sh iproute2.sh ifplugd.sh ip6to4.sh \
		ipppd.sh iwconfig.sh netplugd.sh pppd.sh pump.sh tuntap.sh udhcpc.sh \
		vlan.sh

.SUFFIXES:	.sh.Linux.in
.sh.Linux.in.sh:
	${CP} $< $@
else ifeq ($(OS),NetBSD)
INC+=		ifwatchd.sh

.SUFFIXES:	.sh.BSD.in
.sh.BSD.in.sh:
	${CP} $< $@
endif

include ${MK}/scripts.mk
