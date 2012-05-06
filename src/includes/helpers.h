/*
  helpers.h
  This is private to us and not for user consumption
*/

/*
 * Copyright (c) 2007-2009 Roy Marples <roy@marples.name>
 *
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

#ifndef __HELPERS_H__
#define __HELPERS_H__

#define ERRX fprintf (stderr, "out of memory\n"); exit (1)

#define UNCONST(a)		((void *)(unsigned long)(const void *)(a))

#ifdef lint
# define _unused
#endif
#if __GNUC__ > 2 || defined(__INTEL_COMPILER)
# define _dead __attribute__((__noreturn__))
# define _unused __attribute__((__unused__))
#else
# define _dead
# define _unused
#endif

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

/* Some libc implemntations don't have these */
#ifndef TAILQ_CONCAT
#define TAILQ_CONCAT(head1, head2, field) do {				      \
		if (!TAILQ_EMPTY(head2)) {				      \
			*(head1)->tqh_last = (head2)->tqh_first;	      \
			(head2)->tqh_first->field.tqe_prev = (head1)->tqh_last; \
			(head1)->tqh_last = (head2)->tqh_last;		      \
			TAILQ_INIT((head2));				      \
		}							      \
	} while (0)
#endif

#ifndef TAILQ_FOREACH_SAFE
#define	TAILQ_FOREACH_SAFE(var, head, field, tvar)			      \
	for ((var) = TAILQ_FIRST((head));				      \
	     (var) && ((tvar) = TAILQ_NEXT((var), field), 1);		      \
	     (var) = (tvar))
#endif

#ifdef __GLIBC__
#  if ! defined (__UCLIBC__) && ! defined (__dietlibc__)
#    define strlcpy(dst, src, size) snprintf(dst, size, "%s", src)
#  endif
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
