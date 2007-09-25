/*!
 * @file einfo.h 
 * @brief Describes how to interface with the einfo library
 *
 * Copyright 2007 Gentoo Foundation
 * Released under the GPLv2
 */

#ifndef __EINFO_H__
#define __EINFO_H__

#define EINFO_PRINTF
#define EINFO_XPRINTF
#define EEND_PRINTF

#ifdef __GNUC__
#  undef EINFO_PRINTF
#  undef EINFO_XPRINTF
#  undef EEND_PRINTF
#  define EINFO_PRINTF  __attribute__ ((__format__ (__printf__, 1, 2)))
#  define EINFO_XPRINTF  __attribute__ ((__noreturn__, __format__ (__printf__, 1, 2)))
#  define EEND_PRINTF  __attribute__ ((__format__ (__printf__, 2, 3)))
#endif

#include <sys/types.h>
#include <stdbool.h>

/*! @brief Color types to use */
typedef enum
{
	ecolor_good,
	ecolor_warn,
	ecolor_bad,
	ecolor_hilite,
	ecolor_bracket,
	ecolor_normal
} einfo_color_t;

/*! @brief Returns the ASCII code for the color */
const char *ecolor (einfo_color_t);

/*! @brief Writes to syslog. */
void elog (int level, const char *fmt, ...) EEND_PRINTF;

/*!
 * @brief Display informational messages.
 *
 * The einfo family of functions display messages in a consistent manner
 * across Gentoo applications. Basically they prefix the message with
 * " * ". If the terminal can handle color then we color the * based on
 * the command used. Otherwise we are identical to the printf function.
 *
 * - einfo  - green
 * - ewarn  - yellow
 * - eerror - red
 *
 * The n suffix denotes that no new line should be printed.
 * The v suffix means only print if RC_VERBOSE is yes.
 */
/*@{*/
int einfon (const char *fmt, ...) EINFO_PRINTF;
int ewarnn (const char *fmt, ...) EINFO_PRINTF;
int eerrorn (const char *fmt, ...) EINFO_PRINTF;
int einfo (const char *fmt, ...) EINFO_PRINTF;
int ewarn (const char *fmt, ...) EINFO_PRINTF;
void ewarnx (const char *fmt, ...) EINFO_XPRINTF;
int eerror (const char *fmt, ...) EINFO_PRINTF;
void eerrorx (const char *fmt, ...) EINFO_XPRINTF;

int einfovn (const char *fmt, ...) EINFO_PRINTF;
int ewarnvn (const char *fmt, ...) EINFO_PRINTF;
int ebeginvn (const char *fmt, ...) EINFO_PRINTF;
int eendvn (int retval, const char *fmt, ...) EEND_PRINTF;
int ewendvn (int retval, const char *fmt, ...) EEND_PRINTF;
int einfov (const char *fmt, ...) EINFO_PRINTF;
int ewarnv (const char *fmt, ...) EINFO_PRINTF;
/*@}*/

/*! @ingroup ebegin
 * @brief Display informational messages that may take some time.
 *
 * Similar to einfo, but we add ... to the end of the message */
/*@{*/
int ebeginv (const char *fmt, ...) EINFO_PRINTF;
int ebegin (const char *fmt, ...) EINFO_PRINTF;
/*@}*/

/*! @ingroup eend
 * @brief End an ebegin.
 *
 * If you ebegin, you should eend also.
 * eend places [ ok ] or [ !! ] at the end of the terminal line depending on
 * retval (0 or ok, anything else for !!)
 *
 * ebracket allows you to specifiy the position, color and message */
/*@{*/
int eend (int retval, const char *fmt, ...) EEND_PRINTF;
int ewend (int retval, const char *fmt, ...) EEND_PRINTF;
void ebracket (int col, einfo_color_t color, const char *msg);

int eendv (int retval, const char *fmt, ...) EEND_PRINTF;
int ewendv (int retval, const char *fmt, ...) EEND_PRINTF;
/*@}*/

/*! @ingroup eindent
 * @brief Indents the einfo lines.
 *
 * For each indent you should outdent when done */
/*@{*/
void eindent (void);
void eoutdent (void);
void eindentv (void);
void eoutdentv (void);

/*! @brief Prefix each einfo line with something */
void eprefix (const char *prefix);

#endif
