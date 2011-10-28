DIR=	${DOCDIR}/openrc
SRCS=	net.example.in
INC=	net.example

MK=	../mk
include ${MK}/os.mk

%.example.in: %.example${SFX}
	${CP} $< $@

include ${MK}/scripts.mk
