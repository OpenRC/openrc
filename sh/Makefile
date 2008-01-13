DIR=	${RC_LIB}/sh
SRCS=	functions.in rc-functions.in
OBJS=	${SRCS:.in=.sh}
INC=	init-common-post.sh rc-mount.sh ${OBJS}
BIN=	gendepends.sh net.sh runscript.sh

MK=	../mk
# To get NET_LO
include ${MK}/os.mk
include Makefile.${SUBOS}
include ${MK}/scripts.mk

# Ensure that the lib name is correct
.SUFFIXES: .in .sh
.in.sh:
	sed -e 's:/lib/:/${LIBNAME}/:g' $< > $@

INSTALLAFTER=	_installafter
_installafter:
	${INSTALL} -d ${DESTDIR}/etc/init.d
	@# Provide an init script for the loopback interface
	ln -snf ${RC_LIB}/sh/net.sh ${DESTDIR}/etc/init.d/${NET_LO} || exit $$?
	@# Put functions.sh into the init.d dir so 3rd party apps don't have to
	@# be multilib aware
	ln -snf ${RC_LIB}/sh/functions.sh ${DESTDIR}/etc/init.d || exit $$?
