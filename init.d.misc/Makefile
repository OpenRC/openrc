DIR=		/etc/init.d
_SRCS!=		ls *.in
SRCS?=		${_SRCS}$(shell ls *.in)
OBJS=		${SRCS:.in=}

MK=		../mk
include ${MK}/os.mk
include Makefile.${OS}

VARBASE?=	/var

.SUFFIXES: .in

all: ${OBJS} 

.in:
	sed -e 's:@PREFIX@:${PREFIX}:' -e 's:@VARBASE@:${VARBASE}:' $< > $@

install: all
	${INSTALL} -d ${DESTDIR}${DIR} 
	${INSTALL} ${OBJS} ${DESTDIR}${DIR}

clean:
	rm -f ${OBJS}
