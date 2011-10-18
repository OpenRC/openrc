DIR=	${SYSCONFDIR}
CONF=	rc.conf

CLEANFILES+=	rc.conf

MK=	../mk
include ${MK}/os.mk

ifeq ($(OS),FreeBSD)
SED_EXTRA=	-e 's:@TERM@:cons25:g'
SRCS+=		rc.conf.in rc.in rc.shutdown.in
CONF+=		devd.conf
BIN+=		rc rc.shutdown rc.devd
else ifeq ($(OS),Linux)
SED_EXTRA=	-e 's:@TERM@:wsvt25:g'
SRCS+=		rc.conf.in rc.in rc.shutdown.in
else ifeq ($(OS),NetBSD)
SED_EXTRA=	-e 's:@TERM@:wsvt25:g'
SRCS+=		rc.conf.in rc.in rc.shutdown.in
BIN+=		rc rc.shutdown
endif

include ${MK}/scripts.mk

# We can't use "ifndef" here because that treats set-but-empty
# as not-set which is not what we want
MKRCSYS ?= automagicplease
ifeq ($(MKRCSYS),automagicplease)
# If the user isn't picking a default, then have the
# config go with runtime automagic detection #357247
rc.conf: SED_EXTRA += -e '/^rc_sys=""/s:^:\#:'
MKRCSYS =
endif
rc.conf: rc.conf.in rc.conf.${OS}
	${SED} ${SED_REPLACE} ${SED_EXTRA} $^ > $@
