ifeq (${MKTERMCAP},ncurses)
	LTERMCAP:=	$(shell pkg-config ncurses --libs 2> /dev/null)
ifeq ($(LTERMCAP),)
LIBTERMCAP?=	-lncurses
else
LIBTERMCAP?= $(LTERMCAP)
endif
CPPFLAGS+=	-DHAVE_TERMCAP
LDADD+=		${LIBTERMCAP}
else ifeq (${MKTERMCAP},termcap)
LIBTERMCAP?=	-ltermcap
CPPFLAGS+=	-DHAVE_TERMCAP
LDADD+=		${LIBTERMCAP}
else ifneq (${MKTERMCAP},)
$(error If MKTERMCAP is defined, it must be ncurses or termcap)
endif
