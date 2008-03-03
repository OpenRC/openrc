# rules to make a distribution tarball from a git repo
# Copyright 2008 Roy Marples <roy@marples.name>

GITREF?=	HEAD
DISTPREFIX?=	${NAME}-${VERSION}
DISTFILE?=	${DISTPREFIX}.tar.bz2

CLEANFILES+=	${DISTFILE}

dist:
	git archive --prefix=${DISTPREFIX}/ ${GITREF} | bzip2 > ${DISTFILE}
