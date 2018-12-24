DIR=	${LIBEXECDIR}/sh
SRCS=	init.sh.in functions.sh.in gendepends.sh.in \
	openrc-run.sh.in ${SRCS-${OS}}
INC=	rc-mount.sh functions.sh rc-functions.sh runit.sh s6.sh \
		start-stop-daemon.sh supervise-daemon.sh ${INC-${OS}}
BIN=	gendepends.sh init.sh openrc-run.sh ${BIN-${OS}}

INSTALLAFTER=	_installafter

MK=	../mk
include ${MK}/os.mk

SRCS-FreeBSD=
BIN-FreeBSD=

SRCS-Linux=	binfmt.sh.in cgroup-release-agent.sh.in init-early.sh.in \
	migrate-to-run.sh.in
BIN-Linux=	binfmt.sh cgroup-release-agent.sh init-early.sh migrate-to-run.sh
INC-Linux= rc-cgroup.sh

SRCS-NetBSD=
BIN-NetBSD=

include ${MK}/scripts.mk

%.sh: %.sh${SFX}
	${SED} ${SED_REPLACE} ${SED_EXTRA} $< > $@

_installafter:
	${INSTALL} -d ${DESTDIR}/${INITDIR}
	@# Put functions.sh into init for backwards compat
	ln -snf ${LIBEXECDIR}/sh/functions.sh ${DESTDIR}/${INITDIR} || exit $$?

check test::
	./runtests.sh
