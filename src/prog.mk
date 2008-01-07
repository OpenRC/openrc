# rules to build a library
# based on FreeBSD's bsd.prog.mk

# Copyright 2008 Roy Marples

BINDIR?=		/sbin
OBJS+=			${SRCS:.c=.o}

INSTALL?=		install

all: depend ${PROG}

${PROG}: ${SCRIPTS} ${OBJS}
	${CC} ${CFLAGS} ${LDFLAGS} ${PROGLDFLAGS} -o $@ ${OBJS} ${LDADD}

clean: clean-depend
	rm -f ${OBJS} ${PROG} ${CLEANFILES}

include $(TOPDIR)/depend.mk
