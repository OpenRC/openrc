DIR=		/etc
SRCS=		rc.in
OBJS=		${SRCS:.in=}
CONF=		rc.shutdown ${OBJS}


MK=		../mk
include ${MK}/scripts.mk
include Makefile.${OS}

.SUFFIXES:	.in
.in:
	sed -e s':@TERM@:${DEFTERM}:' $< > $@

