/*
   rc-misc.h
   This is private to us and not for user consumption
   */

/*
 * Copyright 2007 Roy Marples
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
#  define LIB "lib"
#endif

#define RC_LEVEL_BOOT           "boot"
#define RC_LEVEL_DEFAULT        "default"

#define RC_LIBDIR               "/" LIB "/rc"
#define RC_SVCDIR               RC_LIBDIR "/init.d"
#define RC_DEPTREE              RC_SVCDIR "/deptree"
#define RC_RUNLEVELDIR          "/etc/runlevels"
#define RC_INITDIR              "/etc/init.d"
#define RC_CONFDIR              "/etc/conf.d"

#define RC_INITDIR_LOCAL        "/usr/local/etc/init.d"
#define RC_CONFDIR_LOCAL        "/usr/local/etc/conf.d"

#define RC_KSOFTLEVEL           RC_SVCDIR "/ksoftlevel"
#define RC_STARTING             RC_SVCDIR "/rc.starting"
#define RC_STOPPING             RC_SVCDIR "/rc.stopping"

#define RC_SVCDIR_STARTING      RC_SVCDIR "/starting"
#define RC_SVCDIR_INACTIVE      RC_SVCDIR "/inactive"
#define RC_SVCDIR_STARTED       RC_SVCDIR "/started"
#define RC_SVCDIR_COLDPLUGGED	RC_SVCDIR "/coldplugged"

#define RC_PLUGINDIR            RC_LIBDIR "/plugins"

#define ERRX					fprintf (stderr, "out of memory\n"); exit (1)

static inline void *xmalloc (size_t size)
{
	void *value = malloc (size);

	if (value)
		return (value);

	ERRX;
}

static inline void *xrealloc (void *ptr, size_t size)
{
	void *value = realloc (ptr, size);

	if (value)
		return (value);

	ERRX;
}

static inline char *xstrdup (const char *str)
{
	char *value;

	if (! str)
		return (NULL);

	value = strdup (str);

	if (value)
		return (value);

	ERRX;
}

#undef ERRX

static inline bool exists (const char *pathname)
{
	struct stat buf;

	return (stat (pathname, &buf) == 0);
}

static inline bool existss (const char *pathname)
{
	struct stat buf;

	return (stat (pathname, &buf) == 0 && buf.st_size != 0);
}
char *rc_conf_value (const char *var);
bool rc_conf_yesno (const char *var);
char **env_filter (void);
char **env_config (void);
bool service_plugable (const char *service);

/* basename_c never modifies the argument. As such, if there is a trailing
 * slash then an empty string is returned. */
static inline const char *basename_c (const char *path) {
	const char *slash = strrchr (path, '/');

	if (slash)
		return (++slash);
	return (path);
}

#endif
