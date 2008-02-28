PROG=		rc
SRCS=		checkpath.c fstabinfo.c mountinfo.c \
		rc-applets.c rc-depend.c rc-logger.c \
		rc-misc.c rc-plugin.c rc-status.c rc-update.c rc.c \
		runscript.c start-stop-daemon.c

CLEANFILES=	version.h

BINDIR=		${PREFIX}/bin
SBINDIR=	${PREFIX}/sbin
LINKDIR=	${PREFIX}/${LIBNAME}/${PROG}

BINLINKS=	rc-status
SBINLINKS=	rc-update runscript start-stop-daemon
RC_BINLINKS=	einfon einfo ewarnn ewarn eerrorn eerror ebegin eend ewend \
		eindent eoutdent esyslog eval_ecolors \
		veinfo vewarn vebegin veend vewend veindent veoutdent \
		service_starting service_started \
		service_stopping service_stopped \
		service_inactive service_wasinactive \
		service_coldplugged service_started_daemon \
		checkpath fstabinfo mountinfo rc-depend \
		service_get_value service_set_value get_options save_options \
		shell_var is_newer_than is_older_than
RC_SBINLINKS=	mark_service_starting mark_service_started \
		mark_service_stopping mark_service_stopped \
		mark_service_inactive mark_service_wasinactive \
		mark_service_coldplugged mark_service_failed \
		rc-abort

LDFLAGS+=	-L../librc -L../libeinfo
LDADD+=		-lutil -lrc -leinfo

MK=		../../mk
include ${MK}/cc.mk
include ${MK}/os.mk
include ${MK}/prog.mk

CFLAGS+=	-I../includes -I../librc -I../libeinfo
CFLAGS+=	-DLIB=\"${LIBNAME}\"

include ${MK}/${MKTERMCAP}.mk
LDADD+=		${LIBDL} ${LIBKVM}
include ${MK}/${MKPAM}.mk

${SRCS}: version.h
version.h:
	sed -n -e 's/^VERSION=[[:space:]]*\([^[:space:]]*\).*/#define VERSION "\1\"/p' ../../Makefile > version.h
	if test -n "${BRANDING}"; then \
		echo "#define BRANDING \"${BRANDING}\"" >> version.h; \
	fi

install: all
	${INSTALL} -d ${DESTDIR}${SBINDIR}
	${INSTALL} -m ${BINMODE} ${PROG} ${DESTDIR}${SBINDIR}
	${INSTALL} -d ${DESTDIR}${BINDIR}
	for x in ${BINLINKS}; do ln -fs ${DESTDIR}${SBINDIR}/${PROG} ${DESTDIR}${BINDIR}/$$x; done
	${INSTALL} -d ${DESTDIR}${SBINDIR}
	for x in ${SBINLINKS}; do ln -fs ${PROG} ${DESTDIR}${SBINDIR}/$$x; done
	${INSTALL} -d ${DESTDIR}${LINKDIR}/bin
	for x in $(RC_BINLINKS); do ln -fs ${DESTDIR}${SBINDIR}/${PROG} ${DESTDIR}${LINKDIR}/bin/$$x; done
	${INSTALL} -d ${DESTDIR}${LINKDIR}/sbin
	for x in ${RC_SBINLINKS}; do ln -fs ${DESTDIR}${SBINDIR}/${PROG} ${DESTDIR}${LINKDIR}/sbin/$$x; done
	if test "${MKPAM}" = pam; then \
		${INSTALL} -d ${DESTDIR}${PAMDIR}; \
		${INSTALL} -m ${PAMMODE} start-stop-daemon.pam ${DESTDIR}${PAMDIR}/start-stop-daemon; \
	fi

ALL_LINKS = $(BINLINKS) $(SBINLINKS) $(RC_BINLINKS) $(RC_SBINLINKS)
CLEANFILES += $(ALL_LINKS)
links: rc
	for l in $(ALL_LINKS) ; do ln -sf rc $$l || exit $$? ; done
