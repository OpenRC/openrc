DIR=	${LIBEXECDIR}/sh
SRCS=	functions.sh.in gendepends.sh.in init-common-post.sh.in \
	rc-functions.sh.in runscript.sh.in
INC=	init-common-post.sh rc-mount.sh functions.sh rc-functions.sh
BIN=	gendepends.sh init.sh runscript.sh

INSTALLAFTER=	_installafter

MK=	../mk
include ${MK}/os.mk
include Makefile.${OS}
include ${MK}/scripts.mk

_installafter:
	${INSTALL} -d ${DESTDIR}/${INITDIR}

check test::
	./runtests.sh
