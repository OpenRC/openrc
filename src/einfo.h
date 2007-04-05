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
  einfo_good,
  einfo_warn,
  einfo_bad,
  einfo_hilite,
  einfo_bracket,
  einfo_normal
} einfo_color_t;

/* Colour codes used by the below functions. */
#define EINFO_GOOD	"\033[32;01m"
#define EINFO_WARN	"\033[33;01m"
#define EINFO_BAD	"\033[31;01m"
#define EINFO_HILITE	"\033[36;01m"
#define EINFO_BRACKET	"\033[34;01m"
#define EINFO_NORMAL	"\033[0m"

/* Handy macros to easily use the above in a custom manner */
#define PEINFO_GOOD	if (colour_terminal ()) printf (EINFO_GOOD)
#define PEINFO_WARN	if (colour_terminal ()) printf (EINFO_WARN)
#define PEINFO_BAD	if (colour_terminal ()) printf (EINFO_BAD)
#define PEINFO_HILITE	if (colour_terminal ()) printf (EINFO_HILITE)
#define PEINFO_BRACKET	if (colour_terminal ()) printf (EINFO_BRACKET)
#define PEINFO_NORMAL	if (colour_terminal ()) printf (EINFO_NORMAL)

/* We work out if the terminal supports colour or not through the use
   of the TERM env var. We cache the reslt in a static bool, so
   subsequent calls are very fast. */
/* The n suffix means that a newline is NOT appended to the string
   The v prefix means that we only print it when RC_VERBOSE=yes */
bool colour_terminal (void);
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

int veinfon (const char *fmt, ...) EINFO_PRINTF (1, 2);
int vewarnn (const char *fmt, ...) EINFO_PRINTF (1, 2);
int vebeginn (const char *fmt, ...) EINFO_PRINTF (1, 2);
int veendn (int retval, const char *fmt, ...) EINFO_PRINTF (2, 3);
int vewendn (int retval, const char *fmt, ...) EINFO_PRINTF (2, 3);
int veinfo (const char *fmt, ...) EINFO_PRINTF (1, 2);
int vewarn (const char *fmt, ...) EINFO_PRINTF (1, 2);
int vebegin (const char *fmt, ...) EINFO_PRINTF (1, 2);
int veend (int retval, const char *fmt, ...) EINFO_PRINTF (2, 3);
int vewend (int retval, const char *fmt, ...) EINFO_PRINTF (2, 3);
void veindent (void);
void veoutdent (void);

/* If RC_EBUFFER is set, then we buffer all the above commands.
   As such, we need to flush the buffer when done. */
void eflush(void);

#endif
