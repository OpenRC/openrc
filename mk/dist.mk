# rules to make a distribution tarball from a git repo
# Copyright (c) 2008 Roy Marples <roy@marples.name>
# Released under the 2-clause BSD license.

GITREF?=	${VERSION}
DISTPREFIX?=	${NAME}-${VERSION}
DISTFILE?=	${DISTPREFIX}.tar.bz2

CLEANFILES+=	${NAME}-*.tar.bz2

CHANGELOG_LIMIT?= --after="1 year ago"

_SNAP_SH=	date -u +%Y%m%d%H%M
_SNAP:=		$(shell ${_SNAP_SH})
SNAP=		${_SNAP}
SNAPDIR=	${DISTPREFIX}-${SNAP}
SNAPFILE=	${SNAPDIR}.tar.bz2

changelog:
	git log ${CHANGELOG_LIMIT} --format=full > ChangeLog

dist:
	git archive --prefix=${DISTPREFIX}/ ${GITREF} | bzip2 > ${DISTFILE}

distcheck: dist
	rm -rf ${DISTPREFIX}
	tar xf ${DISTFILE}
	MAKEFLAGS= $(MAKE) -C ${DISTPREFIX}
	MAKEFLAGS= $(MAKE) -C ${DISTPREFIX} check
	rm -rf ${DISTPREFIX}

snapshot:
	rm -rf /tmp/${SNAPDIR}
	mkdir /tmp/${SNAPDIR}
	cp -RPp * /tmp/${SNAPDIR}
	(cd /tmp/${SNAPDIR}; make clean)
	rm -rf /tmp/${SNAPDIR}/.git 2>/dev/null || true
	tar -cvjpf ${SNAPFILE} -C /tmp ${SNAPDIR}
	rm -rf /tmp/${SNAPDIR}
	ls -l ${SNAPFILE}

snap: snapshot

