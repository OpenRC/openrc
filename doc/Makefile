DIR=	${DOCDIR}/openrc
INC=	net.example

MK=	../mk
include ${MK}/os.mk

ifeq ($(OS),FreeBSD)
SRCS+=		net.example.in

.SUFFIXES:	.BSD.in
.BSD.in:
	${CP} $< $@
else ifeq ($(OS),Linux)
SRCS+=		net.example.in

.SUFFIXES:	.Linux.in
.Linux.in:
	${CP} $< $@
else ifeq ($(OS),NetBSD)
SRCS+=		net.example.in

.SUFFIXES:	.BSD.in
.BSD.in:
	${CP} $< $@
endif

include ${MK}/scripts.mk
