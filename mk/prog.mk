# rules to build a library
# based on FreeBSD's bsd.prog.mk

# Copyright 2008 Roy Marples

OBJS+=			${SRCS:.c=.o}

all: depend ${PROG}

${PROG}: ${SCRIPTS} ${OBJS}
	${CC} ${CFLAGS} ${LDFLAGS} ${PROGLDFLAGS} -o $@ ${OBJS} ${LDADD}

clean:
	rm -f ${OBJS} ${PROG} ${CLEANFILES}

include ${MK}/sys.mk
include ${MK}/depend.mk
