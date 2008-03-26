DIR=	${PREFIX}/${RC_LIB}/net
SRCS=	ifconfig.sh.in
INC=	dhclient.sh dhcpcd.sh ifconfig.sh macchanger.sh macnet.sh \
	ssidnet.sh system.sh wpa_supplicant.sh

MK=	../mk
include ${MK}/os.mk
include Makefile.${OS}
include ${MK}/scripts.mk
