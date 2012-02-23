DIR=	${LIBEXECDIR}/sh
SRCS=	init.sh.in functions.sh.in gendepends.sh.in init-common-post.sh.in \
	rc-functions.sh.in runscript.sh.in tmpfiles.sh.in ${SRCS-${OS}}
INC=	init-common-post.sh rc-mount.sh functions.sh rc-functions.sh
BIN=	gendepends.sh init.sh runscript.sh tmpfiles.sh ${BIN-${OS}}

INSTALLAFTER=	_installafter

MK=	../mk
include ${MK}/os.mk

SRCS-FreeBSD=
BIN-FreeBSD=

SRCS-Linux=	cgroup-release-agent.sh.in init-early.sh.in migrate-to-run.sh.in \
	udhcpc-hook.sh.in
BIN-Linux=	cgroup-release-agent.sh init-early.sh migrate-to-run.sh \
	udhcpc-hook.sh

SRCS-NetBSD=	ifwatchd-carrier.sh.in ifwatchd-nocarrier.sh.in
BIN-NetBSD=	ifwatchd-carrier.sh ifwatchd-nocarrier.sh

include ${MK}/scripts.mk

%.sh: %.sh${SFX}
	${SED} ${SED_REPLACE} ${SED_EXTRA} $< > $@

_installafter:
	${INSTALL} -d ${DESTDIR}/${INITDIR}
	@# Put functions.sh into init for backwards compat
	ln -snf ${LIBEXECDIR}/sh/functions.sh ${DESTDIR}/${INITDIR} || exit $$?

check test::
	./runtests.sh
