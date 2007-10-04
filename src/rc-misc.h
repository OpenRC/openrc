/*
   rc-misc.h
   This is private to us and not for user consumption
   Copyright 2007 Gentoo Foundation
   */

#ifndef __RC_MISC_H__
#define __RC_MISC_H__

#include <sys/stat.h>
#include <errno.h>

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

#define RC_KSOFTLEVEL           RC_SVCDIR "/ksoftlevel"
#define RC_STARTING             RC_SVCDIR "/rc.starting"
#define RC_STOPPING             RC_SVCDIR "/rc.stopping"

#define RC_SVCDIR_STARTING      RC_SVCDIR "/starting"
#define RC_SVCDIR_INACTIVE      RC_SVCDIR "/inactive"
#define RC_SVCDIR_STARTED       RC_SVCDIR "/started"
#define RC_SVCDIR_COLDPLUGGED	RC_SVCDIR "/coldplugged"

#define RC_PLUGINDIR            RC_LIBDIR "/plugins"

/* Max buffer to read a line from a file */
#define RC_LINEBUFFER           4096 

/* Good defaults just incase user has none set */
#define RC_NET_FS_LIST_DEFAULT  "afs cifs coda davfs fuse gfs ncpfs nfs nfs4 ocfs2 shfs smbfs"

#define ERRX					fprintf (stderr, "out of memory\n"); exit (1)

static inline void *rc_xmalloc (size_t size)
{
	void *value = malloc (size);

	if (value)
		return (value);

	ERRX;
}

static inline void *rc_xrealloc (void *ptr, size_t size)
{
	void *value = realloc (ptr, size);

	if (value)
		return (value);

	ERRX;
}

static inline char *rc_xstrdup (const char *str)
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

static inline bool rc_exists (const char *pathname)
{
	struct stat buf;
	int serrno = errno;

	if (stat (pathname, &buf) == 0)
		return (true);

	errno = serrno;
	return (false);
}

#endif
