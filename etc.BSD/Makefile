DIR=		/etc
SRCS=		rc.in rc.shutdown.in
OBJS=		${SRCS:.in=}
CONF=		${OBJS}


MK=		../mk
include ${MK}/scripts.mk
include Makefile.${OS}

.SUFFIXES:	.in
.in:
	sed -e s':@TERM@:${DEFTERM}:' $< > $@

