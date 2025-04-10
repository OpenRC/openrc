/*
 * helpers.h
 * This is private to us and not for user consumption
 */

/*
 * Copyright (c) 2007-2015 The OpenRC Authors.
 * See the Authors file at the top-level directory of this distribution and
 * https://github.com/OpenRC/openrc/blob/HEAD/AUTHORS
 *
 * This file is part of OpenRC. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution and at https://github.com/OpenRC/openrc/blob/HEAD/LICENSE
 * This file may not be copied, modified, propagated, or distributed
 *    except according to the terms contained in the LICENSE file.
 */

#ifndef __HELPERS_H__
#define __HELPERS_H__

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <fcntl.h>

#define ERRX do { fprintf (stderr, "out of memory\n"); exit (1); } while (0)

#define UNCONST(a)		((void *)(uintptr_t)(const void *)(a))

#define RC_UNUSED __attribute__((__unused__))
#define RC_NORETURN __attribute__((__noreturn__))
#define RC_PRINTF(a, b)  __attribute__((__format__(__printf__, a, b)))

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

#ifndef HAVE_STRLCPY
#    define strlcpy(dst, src, size) snprintf(dst, size, "%s", src)
#endif

#ifndef timespecsub
#define	timespecsub(tsp, usp, vsp)					      \
	do {								      \
		(vsp)->tv_sec = (tsp)->tv_sec - (usp)->tv_sec;		      \
		(vsp)->tv_nsec = (tsp)->tv_nsec - (usp)->tv_nsec;	      \
		if ((vsp)->tv_nsec < 0) {				      \
			(vsp)->tv_sec--;				      \
			(vsp)->tv_nsec += 1000000000L;			      \
		}							      \
	} while (/* CONSTCOND */ 0)
#endif

RC_UNUSED static void *xmalloc (size_t size)
{
	void *value = malloc(size);

	if (value)
		return (value);

	ERRX;
	/* NOTREACHED */
}

RC_UNUSED static void *xrealloc(void *ptr, size_t size)
{
	void *value = realloc(ptr, size);

	if (value)
		return (value);

	ERRX;
	/* NOTREACHED */
}

RC_UNUSED static char *xstrdup(const char *str)
{
	char *value;

	if (!str)
		return (NULL);

	value = strdup(str);

	if (value)
		return (value);

	ERRX;
	/* NOTREACHED */
}

RC_UNUSED static FILE *xopen_memstream(char **ptr, size_t *sizeloc)
{
	FILE *f = open_memstream(ptr, sizeloc);
	if (f)
		return f;
	ERRX;
}

RC_UNUSED static void xclose_memstream(FILE *f)
{
	fflush(f);
	if (ferror(f) || fclose(f) != 0)
		ERRX;
}

#undef ERRX

/*
 * basename_c never modifies the argument. As such, if there is a trailing
 * slash then an empty string is returned.
 */
RC_UNUSED static const char *basename_c(const char *path)
{
	const char *slash = strrchr(path, '/');

	if (slash)
		return (++slash);
	return (path);
}

RC_UNUSED static bool existsat(int dirfd, const char *pathname)
{
	struct stat buf;

	return (fstatat(dirfd, pathname, &buf, 0) == 0);
}

RC_UNUSED static bool exists(const char *pathname)
{
	struct stat buf;

	return (stat(pathname, &buf) == 0);
}

RC_UNUSED static bool existss(const char *pathname)
{
	struct stat buf;

	return (stat(pathname, &buf) == 0 && buf.st_size != 0);
}

RC_UNUSED static FILE *do_fopenat(int dirfd, const char *pathname, int mode)
{
	int fd = openat(dirfd, pathname, mode, 0666);
	const char *fmode;
	FILE *fp;

	if (fd == -1)
		return NULL;

	/* O_CREAT and O_TRUNC in modes 'a', 'w+', 'a+' don't
	 * really matter after the file has been opened.
	 *
	 * Some implementations have O_RDONLY defined to zero,
	 * thus making it impossible to detect, so, we default
	 * to "r" if no other mask fully matches. */
	if ((mode & O_RDWR) == O_RDWR)
		fmode = "r+";
	else if ((mode & O_WRONLY) == O_WRONLY)
		fmode = "w";
	else
		fmode = "r";

	if (!(fp = fdopen(fd, fmode)))
		close(fd);
	return fp;
}

RC_UNUSED static DIR *do_opendirat(int dirfd, const char *pathname)
{
	int fd = openat(dirfd, pathname, O_RDONLY | O_DIRECTORY);
	DIR *dp;

	if (fd == -1)
		return NULL;

	if (!(dp = fdopendir(fd)))
		close(fd);

	return dp;
}

RC_UNUSED static DIR *do_dopendir(int dirfd)
{
	int fd = dup(dirfd);
	DIR *dp;

	if (fd == -1)
		return NULL;

	if (!(dp = fdopendir(fd)))
		close(fd);

	return dp;
}

/*
 * This is an OpenRC specific version of the asprintf() function.
 * We do this to avoid defining the _GNU_SOURCE feature test macro on
 * glibc systems and to ensure that we have a consistent function across
 * platforms. This also allows us to call our xmalloc and xrealloc
 * functions to handle memory allocation.
 * this function was originally written by Mike Frysinger.
 */
RC_UNUSED RC_PRINTF(2,3) static int xasprintf(char **strp, const char *fmt, ...)
{
	va_list ap;
	int len;
	int memlen;
	char *ret;

	/*
	 * Start with a buffer size that should cover the vast majority of uses
	 * (path construction).
	 */
	memlen = 4096;
	ret = xmalloc(memlen);

	va_start(ap, fmt);
	len = vsnprintf(ret, memlen, fmt, ap);
	va_end(ap);
	if (len >= memlen) {
		/*
		 * Output was truncated, so increase buffer to exactly what we need.
		 */
		memlen = len + 1;
		ret = xrealloc(ret, memlen);
		va_start(ap, fmt);
		len = vsnprintf(ret, len + 1, fmt, ap);
		va_end(ap);
	}
	if (len < 0 || len >= memlen) {
		/* Give up! */
		fprintf(stderr, "xasprintf: unable to format a buffer\n");
		free(ret);
		exit(1);
	}
	*strp = ret;
	return len;
}

RC_UNUSED static ssize_t xgetline(char **restrict lineptr, size_t *restrict n, FILE *restrict stream)
{
	ssize_t ret = getline(lineptr, n, stream);
	if (ret <= 0)
		return ret;

	if ((*lineptr)[ret - 1] == '\n')
		(*lineptr)[--ret] = '\0';

	return ret;
}

#endif
