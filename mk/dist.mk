# rules to make a distribution tarball from a git repo
# Copyright 2008 Roy Marples <roy@marples.name>
# All rights reserved. Released under the 2-clause BSD license.

DISTPREFIX?=	${NAME}-${VERSION}
DISTFILE?=	${DISTPREFIX}.tar.bz2

CLEANFILES+=	${DISTFILE}

dist:
	svn export . ${DISTPREFIX}
	tar cjpf ${DISTFILE} ${DISTPREFIX}
	rm -rf ${DISTPREFIX}
