# rules to make a distribution tarball from a git repo
# Copyright 2008 Roy Marples <roy@marples.name>
# All rights reserved. Released under the 2-clause BSD license.

DISTPREFIX?=	${NAME}-${VERSION}
DISTFILE?=	${DISTPREFIX}.tar.bz2

CLEANFILES+=	${NAME}-*.tar.bz2

_SNAP_SH=	date -u +%Y%m%d%H%M
_SNAP!=		${_SNAP_SH}
SNAP=		${_SNAP}$(shell ${_SNAP_SH})
SNAPDIR=	${DISTPREFIX}-${SNAP}
SNAPFILE=	${SNAPDIR}.tar.bz2

dist:
	svn export . ${DISTPREFIX}
	tar cjpf ${DISTFILE} ${DISTPREFIX}
	rm -rf ${DISTPREFIX}

snapshot:
	rm -rf /tmp/${SNAPDIR}
	mkdir /tmp/${SNAPDIR}
	cp -RPp * /tmp/${SNAPDIR}
	(cd /tmp/${SNAPDIR}; make clean)
	find /tmp/${SNAPDIR} -name .svn -exec rm -rf -- {} \; 2>/dev/null || true
	tar -cvjpf ${SNAPFILE} -C /tmp ${SNAPDIR}
	rm -rf /tmp/${SNAPDIR}
	ls -l ${SNAPFILE}

snap: snapshot

