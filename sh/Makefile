DIR=	${PREFIX}/${RC_LIB}/sh
SRCS=	functions.sh.in gendepends.sh.in net.sh.in \
	rc-functions.sh.in runscript.sh.in
INC=	init-common-post.sh rc-mount.sh functions.sh rc-functions.sh
BIN=	gendepends.sh init.sh net.sh runscript.sh

INSTALLAFTER=	_installafter

MK=	../mk
include ${MK}/os.mk
include Makefile.${OS}
include ${MK}/scripts.mk

_installafter:
	${INSTALL} -d ${DESTDIR}/${INITDIR}
	@# Provide an init script for the loopback interface
	ln -snf ${PREFIX}/${RC_LIB}/sh/net.sh ${DESTDIR}/${INITDIR}/${NET_LO} || exit $$?
	@# Put functions.sh into the init.d dir so 3rd party apps don't have to
	@# be multilib aware
	ln -snf ${PREFIX}/${RC_LIB}/sh/functions.sh ${DESTDIR}/${INITDIR} || exit $$?

check test::
	./runtests.sh
