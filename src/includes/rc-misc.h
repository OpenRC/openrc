/*
   rc-misc.h
   This is private to us and not for user consumption
   */

/*
 * Copyright 2007-2008 Roy Marples
 * All rights reserved

 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef __RC_MISC_H__
#define __RC_MISC_H__

#include <sys/stat.h>
#include <errno.h>
#include <stdbool.h>
#include <string.h>

#ifndef LIB
#  define LIB			"lib"
#endif

#ifdef PREFIX
#  define RC_PREFIX		PREFIX
#else
#  define RC_PREFIX
#endif

#ifndef SYSCONFDIR
#  define SYSCONFDIR		RC_PREFIX "/etc"
#endif

#define RC_LEVEL_BOOT           "boot"
#define RC_LEVEL_DEFAULT        "default"

#define RC_LIBDIR               RC_PREFIX "/" LIB "/rc"
#define RC_SVCDIR               RC_LIBDIR "/init.d"
#define RC_DEPTREE_CACHE        RC_SVCDIR "/deptree"
#define RC_RUNLEVELDIR          SYSCONFDIR "/runlevels"
#define RC_INITDIR              SYSCONFDIR "/init.d"
#define RC_CONFDIR              SYSCONFDIR "/conf.d"

/* PKG_PREFIX is where packages are installed if different from the base OS
 * On Gentoo this is normally unset, on FreeBSD /usr/local and on NetBSD
 * /usr/pkg. */
#ifdef PKG_PREFIX
#  define RC_PKG_INITDIR        PKG_PREFIX "/etc/init.d"
#  define RC_PKG_CONFDIR        PKG_PREFIX "/etc/conf.d"
#endif

/* LOCAL_PREFIX is for user written stuff, which the base OS and package
 * manger don't touch. */
#ifdef LOCAL_PREFIX
#  define RC_LOCAL_INITDIR      LOCAL_PREFIX "/etc/init.d"
#  define RC_LOCAL_CONFDIR      LOCAL_PREFIX "/etc/conf.d"
#endif

#define RC_KRUNLEVEL            RC_SVCDIR "/krunlevel"
#define RC_STARTING             RC_SVCDIR "/rc.starting"
#define RC_STOPPING             RC_SVCDIR "/rc.stopping"

#define RC_SVCDIR_STARTING      RC_SVCDIR "/starting"
#define RC_SVCDIR_INACTIVE      RC_SVCDIR "/inactive"
#define RC_SVCDIR_STARTED       RC_SVCDIR "/started"
#define RC_SVCDIR_COLDPLUGGED	RC_SVCDIR "/coldplugged"

#define RC_PLUGINDIR            RC_LIBDIR "/plugins"

#define ERRX fprintf (stderr, "out of memory\n"); exit (1)

#ifdef lint
# define _unused
#endif
#if __GNUC__ > 2 || defined(__INTEL_COMPILER)
# define _unused __attribute__((__unused__))
#else
# define _unused
#endif


/* Some libc implemntations don't have these */
#ifndef STAILQ_CONCAT
#define	STAILQ_CONCAT(head1, head2) do {				\
	if (!STAILQ_EMPTY((head2))) {					\
		*(head1)->stqh_last = (head2)->stqh_first;		\
		(head1)->stqh_last = (head2)->stqh_last;		\
		STAILQ_INIT((head2));					\
	}								\
} while (0)
#endif

#ifndef TAILQ_CONCAT
#define TAILQ_CONCAT(head1, head2, field) do {                          \
	if (!TAILQ_EMPTY(head2)) {                                      \
		*(head1)->tqh_last = (head2)->tqh_first;                \
		(head2)->tqh_first->field.tqe_prev = (head1)->tqh_last; \
		(head1)->tqh_last = (head2)->tqh_last;                  \
		TAILQ_INIT((head2));                                    \
	}                                                               \
} while (0)
#endif

#ifndef STAILQ_FOREACH_SAFE
#define	STAILQ_FOREACH_SAFE(var, head, field, tvar)			\
	for ((var) = STAILQ_FIRST((head));				\
	     (var) && ((tvar) = STAILQ_NEXT((var), field), 1);		\
	     (var) = (tvar))
#endif

#ifndef TAILQ_FOREACH_SAFE
#define	TAILQ_FOREACH_SAFE(var, head, field, tvar)			\
	for ((var) = TAILQ_FIRST((head));				\
	     (var) && ((tvar) = TAILQ_NEXT((var), field), 1);		\
	     (var) = (tvar))
#endif


_unused static void *xmalloc (size_t size)
{
	void *value = malloc(size);

	if (value)
		return (value);

	ERRX;
	/* NOTREACHED */
}

_unused static void *xrealloc(void *ptr, size_t size)
{
	void *value = realloc(ptr, size);

	if (value)
		return (value);

	ERRX;
	/* NOTREACHED */
}

_unused static char *xstrdup(const char *str)
{
	char *value;

	if (! str)
		return (NULL);

	value = strdup(str);

	if (value)
		return (value);

	ERRX;
	/* NOTREACHED */
}

#undef ERRX

_unused static bool exists(const char *pathname)
{
	struct stat buf;

	return (stat(pathname, &buf) == 0);
}

_unused static bool existss(const char *pathname)
{
	struct stat buf;

	return (stat(pathname, &buf) == 0 && buf.st_size != 0);
}

char *rc_conf_value(const char *var);
bool rc_conf_yesno(const char *var);
void env_filter(void);
void env_config(void);
bool service_plugable(const char *service);
int signal_setup(int sig, void (*handler)(int));

/* basename_c never modifies the argument. As such, if there is a trailing
 * slash then an empty string is returned. */
_unused static const char *basename_c(const char *path)
{
	const char *slash = strrchr(path, '/');

	if (slash)
		return (++slash);
	return (path);
}

#endif
