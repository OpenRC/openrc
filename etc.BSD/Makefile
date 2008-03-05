DIR=		${SYSCONFDIR}
CONF=		rc rc.shutdown

MK=		../mk
include ${MK}/scripts.mk
include Makefile.${OS}

.SUFFIXES:	.in
.in:
	sed -e 's:@SHELL@:${SHELL}:' -e 's:@TERM@:${DEFTERM}:' $< > $@
