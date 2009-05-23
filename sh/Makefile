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
	@# Put functions.sh into init for backwards compat
	ln -snf ${LIBEXECDIR}/sh/functions.sh ${DESTDIR}/${INITDIR} || exit $$?

check test::
	./runtests.sh
