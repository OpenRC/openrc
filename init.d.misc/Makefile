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
	sed -e s':@PREFIX@:${PREFIX}:' -e 's:@VARBASE@:${VARBASE}:' $< > $@

install: all 
	for x in ${OBJS}; do \
		${INSTALL} work/$$x ${DESTDIR}${DIR}; \
	done

clean:
	rm -f ${OBJS}
