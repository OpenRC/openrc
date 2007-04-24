/*
   rc.h 
   Header file for external applications to get RC information.
   Copyright 2007 Gentoo Foundation
   Released under the GPLv2
   */

#ifndef __EINFO_H__
#define __EINFO_H__

#ifdef __GNUC__
#  define EINFO_PRINTF(_one, _two)  __attribute__ ((__format__ (__printf__, _one, _two)))
#  define EINFO_XPRINTF(_one, _two)  __attribute__ ((__noreturn__, __format__ (__printf__, _one, _two)))
#endif

#include <sys/types.h>
#include <stdbool.h>

typedef enum
{
	ecolor_good,
	ecolor_warn,
	ecolor_bad,
	ecolor_hilite,
	ecolor_bracket,
	ecolor_normal
} einfo_color_t;

/* We work out if the terminal supports colour or not through the use
   of the TERM env var. We cache the reslt in a static bool, so
   subsequent calls are very fast.
   The n suffix means that a newline is NOT appended to the string
   The v suffix means that we only print it when RC_VERBOSE=yes
   NOTE We use the v suffix here so we can add veinfo for va_list
   in the future, but veinfo is used by shell scripts as they don't
   have the va_list concept
   */
const char *ecolor (einfo_color_t);
int einfon (const char *fmt, ...) EINFO_PRINTF (1, 2);
int ewarnn (const char *fmt, ...) EINFO_PRINTF (1, 2);
int eerrorn (const char *fmt, ...) EINFO_PRINTF (1, 2);
int einfo (const char *fmt, ...) EINFO_PRINTF(1, 2);
int ewarn (const char *fmt, ...) EINFO_PRINTF (1, 2);
void ewarnx (const char *fmt, ...) EINFO_XPRINTF (1,2);
int eerror (const char *fmt, ...) EINFO_PRINTF (1,2);
void eerrorx (const char *fmt, ...) EINFO_XPRINTF (1,2);
int ebegin (const char *fmt, ...) EINFO_PRINTF (1, 2);
int eend (int retval, const char *fmt, ...) EINFO_PRINTF (2, 3);
int ewend (int retval, const char *fmt, ...) EINFO_PRINTF (2, 3);
void ebracket (int col, einfo_color_t color, const char *msg);
void eindent (void);
void eoutdent (void);

int einfovn (const char *fmt, ...) EINFO_PRINTF (1, 2);
int ewarnvn (const char *fmt, ...) EINFO_PRINTF (1, 2);
int ebeginvn (const char *fmt, ...) EINFO_PRINTF (1, 2);
int eendvn (int retval, const char *fmt, ...) EINFO_PRINTF (2, 3);
int ewendvn (int retval, const char *fmt, ...) EINFO_PRINTF (2, 3);
int einfov (const char *fmt, ...) EINFO_PRINTF (1, 2);
int ewarnv (const char *fmt, ...) EINFO_PRINTF (1, 2);
int ebeginv (const char *fmt, ...) EINFO_PRINTF (1, 2);
int eendv (int retval, const char *fmt, ...) EINFO_PRINTF (2, 3);
int ewendv (int retval, const char *fmt, ...) EINFO_PRINTF (2, 3);
void eindentv (void);
void eoutdentv (void);

/* Handy utils to buffer stdout and stderr so our output is always
 * sane when forking around.
 * Don't depend on these being here though as we may take a different
 * approach at a later date. */
void ebuffer (const char *file);
void eflush (void);
void eclose (void);

#endif
