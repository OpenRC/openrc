DIR=	${RC_LIB}/sh
SRCS=	functions.sh.in gendepends.sh.in net.sh.in \
	rc-functions.sh.in runscript.sh.in
INC=	init-common-post.sh rc-mount.sh functions.sh rc-functions.sh
BIN=	gendepends.sh net.sh runscript.sh

INSTALLAFTER=	_installafter

MK=	../mk
# To get NET_LO
include ${MK}/scripts.mk
include Makefile.${SUBOS}

_installafter:
	${INSTALL} -d ${DESTDIR}/${PREFIX}/${INITDIR}
	@# Provide an init script for the loopback interface
	ln -snf ${PREFIX}/${RC_LIB}/sh/net.sh ${DESTDIR}/${PREFIX}/${INITDIR}/${NET_LO} || exit $$?
	@# Put functions.sh into the init.d dir so 3rd party apps don't have to
	@# be multilib aware
	ln -snf ${PREFIX}/${RC_LIB}/sh/functions.sh ${DESTDIR}/${PREFIX}/${INITDIR} || exit $$?
