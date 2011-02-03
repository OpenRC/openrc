ifeq (${MKPAM},pam)
LIBPAM?=	-lpam
CPPFLAGS+=	-DHAVE_PAM
LDADD+=		${LIBPAM}

PAMDIR?=	/etc/pam.d
PAMMODE?=	0644
else ifneq (${MKPAM},)
$(error if MKPAM is defined, it must be "pam")
endif
