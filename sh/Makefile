DIR=	${LIBEXECDIR}/sh
SRCS=	functions.sh.in gendepends.sh.in init-common-post.sh.in \
	rc-functions.sh.in runscript.sh.in
INC=	init-common-post.sh rc-mount.sh functions.sh rc-functions.sh
BIN=	gendepends.sh init.sh runscript.sh

INSTALLAFTER=	_installafter

MK=	../mk
include ${MK}/os.mk

ifeq ($(OS),FreeBSD)
SRCS+=		init.sh.in

.SUFFIXES:	.sh.BSD.in
.sh.BSD.in.sh:
	${SED} ${SED_REPLACE} ${SED_EXTRA} $< > $@

else ifeq ($(OS),Linux)
SRCS+=		init.sh.in init-early.sh.in udhcpc-hook.sh.in
BIN+=		init-early.sh udhcpc-hook.sh

.SUFFIXES:	.sh.Linux.in
.sh.Linux.in.sh:
	${SED} ${SED_REPLACE} ${SED_EXTRA} $< > $@

else ifeq ($(OS),NetBSD)
SRCS+=		init.sh.in

SRCS+=		ifwatchd-carrier.sh.in ifwatchd-nocarrier.sh.in
BIN+=		ifwatchd-carrier.sh ifwatchd-nocarrier.sh

.SUFFIXES:	.sh.BSD.in
.sh.BSD.in.sh:
	${SED} ${SED_REPLACE} ${SED_EXTRA} $< > $@
endif

include ${MK}/scripts.mk

_installafter:
	${INSTALL} -d ${DESTDIR}/${INITDIR}
	@# Put functions.sh into init for backwards compat
	ln -snf ${LIBEXECDIR}/sh/functions.sh ${DESTDIR}/${INITDIR} || exit $$?

check test::
	./runtests.sh
