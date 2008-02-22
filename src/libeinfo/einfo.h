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

#ifndef __EINFO_H__
#define __EINFO_H__

#if defined(__GNUC__)
# define __EINFO_PRINTF  __attribute__ ((__format__ (__printf__, 1, 2)))
# define __EINFO_XPRINTF __attribute__ ((__noreturn__, __format__ (__printf__, 1, 2)))
# define __EEND_PRINTF   __attribute__ ((__format__ (__printf__, 2, 3)))
#else
# define __EINFO_PRINTF
# define __EINFO_XPRINTF
# define __EEND_PRINTF
#endif

#include <sys/types.h>
#include <stdbool.h>

/* Although OpenRC requires C99, linking to us should not. */
#ifdef restrict
# define __EINFO_RESTRICT restrict
#else
# ifdef __restrict
#  define __EINFO_RESTRICT __restrict
# else
#  define __EINFO_RESTRICT
# endif
#endif

/*! @brief Color types to use */
typedef enum
{
	ECOLOR_NORMAL = 1,
	ECOLOR_GOOD = 2,
	ECOLOR_WARN = 3,
	ECOLOR_BAD = 4,
	ECOLOR_HILITE = 5,
	ECOLOR_BRACKET = 6
} einfo_color_t;

/*! @brief Returns the ASCII code for the color */
const char *ecolor (einfo_color_t);

/*! @brief Writes to syslog. */
void elog (int __level, const char * __EINFO_RESTRICT __fmt, ...) __EEND_PRINTF;

/*!
 * @brief Display informational messages.
 *
 * The einfo family of functions display messages in a consistent manner
 * across applications. Basically they prefix the message with
 * " * ". If the terminal can handle color then we color the * based on
 * the command used. Otherwise we are identical to the printf function.
 *
 * - einfo  - green
 * - ewarn  - yellow
 * - eerror - red
 *
 * The n suffix denotes that no new line should be printed.
 * The v suffix means only print if EINFO_VERBOSE is yes.
 */
/*@{*/
int einfon (const char * __EINFO_RESTRICT __fmt, ...) __EINFO_PRINTF;
int ewarnn (const char * __EINFO_RESTRICT __fmt, ...) __EINFO_PRINTF;
int eerrorn (const char * __EINFO_RESTRICT __fmt, ...) __EINFO_PRINTF;
int einfo (const char * __EINFO_RESTRICT __fmt, ...) __EINFO_PRINTF;
int ewarn (const char * __EINFO_RESTRICT __fmt, ...) __EINFO_PRINTF;
void ewarnx (const char * __EINFO_RESTRICT __fmt, ...) __EINFO_XPRINTF;
int eerror (const char * __EINFO_RESTRICT __fmt, ...) __EINFO_PRINTF;
void eerrorx (const char * __EINFO_RESTRICT __fmt, ...) __EINFO_XPRINTF;

int einfovn (const char * __EINFO_RESTRICT __fmt, ...) __EINFO_PRINTF;
int ewarnvn (const char * __EINFO_RESTRICT __fmt, ...) __EINFO_PRINTF;
int ebeginvn (const char * __EINFO_RESTRICT __fmt, ...) __EINFO_PRINTF;
int eendvn (int __retval, const char * __EINFO_RESTRICT __fmt, ...) __EEND_PRINTF;
int ewendvn (int __retval, const char * __EINFO_RESTRICT __fmt, ...) __EEND_PRINTF;
int einfov (const char * __EINFO_RESTRICT __fmt, ...) __EINFO_PRINTF;
int ewarnv (const char * __EINFO_RESTRICT __fmt, ...) __EINFO_PRINTF;
/*@}*/

/*! @ingroup ebegin
 * @brief Display informational messages that may take some time.
 *
 * Similar to einfo, but we add ... to the end of the message */
/*@{*/
int ebeginv (const char * __EINFO_RESTRICT __fmt, ...) __EINFO_PRINTF;
int ebegin (const char * __EINFO_RESTRICT __fmt, ...) __EINFO_PRINTF;
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
int eend (int __retval, const char * __EINFO_RESTRICT __fmt, ...) __EEND_PRINTF;
int ewend (int __retval, const char * __EINFO_RESTRICT __fmt, ...) __EEND_PRINTF;
void ebracket (int __col, einfo_color_t __color, const char * __EINFO_RESTRICT __msg);

int eendv (int __retval, const char * __EINFO_RESTRICT __fmt, ...) __EEND_PRINTF;
int ewendv (int __retval, const char * __EINFO_RESTRICT __fmt, ...) __EEND_PRINTF;
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
void eprefix (const char * __EINFO_RESTRICT __prefix);

#endif
