PROG=		openrc
SRCS=		checkpath.c fstabinfo.c mountinfo.c openrc-run.c \
		rc-applets.c rc-depend.c rc-logger.c \
		rc-misc.c rc-plugin.c rc-service.c rc-status.c rc-update.c \
		rc.c start-stop-daemon.c swclock.c

ifeq (${MKSELINUX},yes)
SRCS+=		rc-selinux.c
endif

CLEANFILES=	version.h rc-selinux.o

BINDIR=		${PREFIX}/bin
SBINDIR=	${PREFIX}/sbin
LINKDIR=	${LIBEXECDIR}

BINLINKS=	rc-status
SBINLINKS=	rc rc-service rc-update openrc-run runscript service \
			start-stop-daemon
RC_BINLINKS=	einfon einfo ewarnn ewarn eerrorn eerror ebegin eend ewend \
		eindent eoutdent esyslog eval_ecolors ewaitfile \
		veinfo vewarn vebegin veend vewend veindent veoutdent \
		service_starting service_started \
		service_stopping service_stopped \
		service_inactive service_wasinactive \
		service_hotplugged service_started_daemon service_crashed \
		checkpath fstabinfo mountinfo rc-depend \
		service_get_value service_set_value get_options save_options \
		shell_var is_newer_than is_older_than
RC_SBINLINKS=	mark_service_starting mark_service_started \
		mark_service_stopping mark_service_stopped \
		mark_service_inactive mark_service_wasinactive \
		mark_service_hotplugged mark_service_failed \
		rc-abort swclock
ALL_LINKS=	${BINLINKS} ${SBINLINKS} ${RC_BINLINKS} ${RC_SBINLINKS}
CLEANFILES+=	${ALL_LINKS}

LOCAL_CPPFLAGS=-I../includes -I../librc -I../libeinfo
LOCAL_LDFLAGS=-L../librc -L../libeinfo
LDADD+=		-lutil -lrc -leinfo

include ../../Makefile.inc
MK=		../../mk
include ${MK}/prog.mk
include ${MK}/gitver.mk
include ${MK}/cc.mk

include ${MK}/termcap.mk
LDADD+=		${LIBDL} ${LIBKVM}
include ${MK}/pam.mk

# create symlinks to rc if not an SELINUX system, otherwise create a wrapper
# script to call rc with the proper name of the applet to execute.
# $1 is a list of the links
# $2 is the path+name of the target to link to (usually 'rc' or '/sbin/rc')
# $3 is the path where the links are created
define make-links
	for x in $(1); do \
		if [ "${MKSELINUX}" = yes ]; then \
			printf '#!/bin/sh\nexec ${2} --applet %s "$$@"\n' $$x >${3}/$$x; \
			chmod ${BINMODE} ${3}/$$x; \
		else \
			ln -sf ${2} ${3}/$$x; \
		fi; \
	done;
endef

${SRCS}: version.h

.PHONY:	version.h.tmp
version.h.tmp:
	echo "#define VERSION \"${VERSION}${GITVER}\"" >$@
	if test -n "${BRANDING}"; then \
		echo "#define BRANDING \"${BRANDING}\"" >> $@; \
	fi

version.h: version.h.tmp
	cmp -s $@.tmp $@ && rm $@.tmp || mv $@.tmp $@

install: all
	${INSTALL} -d ${DESTDIR}${SBINDIR}
	${INSTALL} -m ${BINMODE} ${PROG} ${DESTDIR}${SBINDIR}
	${INSTALL} -d ${DESTDIR}${BINDIR}
	$(call make-links,${BINLINKS},${SBINDIR}/${PROG},${DESTDIR}${BINDIR})
	${INSTALL} -d ${DESTDIR}${SBINDIR}
	$(call make-links,${SBINLINKS},${SBINDIR}/${PROG},${DESTDIR}${SBINDIR})
	${INSTALL} -d ${DESTDIR}${LINKDIR}/bin
	$(call make-links,${RC_BINLINKS},${SBINDIR}/${PROG},${DESTDIR}${LINKDIR}/bin)
	${INSTALL} -d ${DESTDIR}${LINKDIR}/sbin
	$(call make-links, ${RC_SBINLINKS},${SBINDIR}/${PROG},${DESTDIR}${LINKDIR}/sbin)
	if test "${MKPAM}" = pam; then \
		${INSTALL} -d ${DESTDIR}${PAMDIR}; \
		${INSTALL} -m ${PAMMODE} start-stop-daemon.pam ${DESTDIR}${PAMDIR}/start-stop-daemon; \
	fi

check test::

links: ${PROG}
	$(call make-links,${ALL_LINKS},${PROG},.)
