ifeq (${MKTERMCAP},ncurses)
LIBTERMCAP?=	-lncurses
CPPFLAGS+=	-DHAVE_TERMCAP
LDADD+=		${LIBTERMCAP}
else ifeq (${MKTERMCAP},termcap)
LIBTERMCAP?=	-ltermcap
CPPFLAGS+=	-DHAVE_TERMCAP
LDADD+=		${LIBTERMCAP}
else ifneq (${MKTERMCAP},)
$(error If MKTERMCAP is defined, it must be ncurses or termcap)
endif
