/*
   einfo.c
   Informational functions
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

const char libeinfo_copyright[] = "Copyright (c) 2007-2008 Roy Marples";

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <syslog.h>
#ifdef HAVE_TERMCAP
# include <termcap.h>
#endif
#include <unistd.h>

#include "einfo.h"
#include "hidden-visibility.h"
hidden_proto(ecolor)
hidden_proto(ebegin)
hidden_proto(ebeginv)
hidden_proto(ebracket)
hidden_proto(eend)
hidden_proto(eendv)
hidden_proto(eerror)
hidden_proto(eerrorn)
hidden_proto(eerrorx)
hidden_proto(eindent)
hidden_proto(eindentv)
hidden_proto(einfo)
hidden_proto(einfon)
hidden_proto(einfov)
hidden_proto(einfovn)
hidden_proto(elog)
hidden_proto(eoutdent)
hidden_proto(eoutdentv)
hidden_proto(eprefix)
hidden_proto(ewarn)
hidden_proto(ewarnn)
hidden_proto(ewarnv)
hidden_proto(ewarnvn)
hidden_proto(ewarnx)
hidden_proto(ewend)
hidden_proto(ewendv)

/* Incase we cannot work out how many columns from ioctl, supply a default */
#define DEFAULT_COLS		 80

#define OK			"ok"
#define NOT_OK			"!!"

/* Number of spaces for an indent */
#define INDENT_WIDTH		2

/* How wide can the indent go? */
#define INDENT_MAX		40

/* Default colours */
#define GOOD                    2
#define WARN                    3
#define BAD                     1
#define HILITE                  6
#define BRACKET                 4

/* We fallback to these escape codes if termcap isn't available
 * like say /usr isn't mounted */
#define AF "\033[3%dm"
#define CE "\033[K"
#define CH "\033[%dC"
#define MD "\033[1m"
#define ME "\033[m"
#define UP "\033[A"

#define _GET_CAP(_d, _c) strlcpy (_d, tgoto (_c, 0, 0), sizeof (_d));
#define _ASSIGN_CAP(_v) { \
	_v = p; \
	p += strlcpy (p, tmp, sizeof (ebuffer) - (p - ebuffer)) + 1; \
}

/* A pointer to a string to prefix to einfo/ewarn/eerror messages */
static const char *_eprefix = NULL;

/* Buffers and structures to hold the final colours */
static char ebuffer[100];
struct ecolor {
	einfo_color_t color;
	int def;
	const char *name;
	char *str;
};
static char nullstr = '\0';

static struct ecolor ecolors[] = {
	{ ECOLOR_GOOD,    GOOD,    "good",    NULL },
	{ ECOLOR_WARN,    WARN,    "warn",    NULL },
	{ ECOLOR_BAD,     BAD,     "bad",     NULL },
	{ ECOLOR_HILITE,  HILITE,  "hilite",  NULL },
	{ ECOLOR_BRACKET, BRACKET, "bracket", NULL },
	{ ECOLOR_NORMAL,  0,       NULL,      NULL },
};

static char *flush = NULL;
static char *up = NULL;
static char *goto_column = NULL;

static const char *term = NULL;
static bool term_is_cons25 = false;

/* Termcap buffers and pointers
 * Static buffers suck hard, but some termcap implementations require them */
#ifdef HAVE_TERMCAP
static char termcapbuf[2048];
static char tcapbuf[512];
#else
/* No curses support, so we hardcode a list of colour capable terms */
static const char *const color_terms[] = {
	"Eterm",
	"ansi",
	"color-xterm",
	"con132x25",
	"con132x30",
	"con132x43",
	"con132x60",
	"con80x25",
	"con80x28",
	"con80x30",
	"con80x43",
	"con80x50",
	"con80x60",
	"cons25",
	"console",
	"cygwin",
	"dtterm",
	"gnome",
	"konsole",
	"kterm",
	"linux",
	"linux-c",
	"mach-color",
	"mlterm",
	"putty",
	"rxvt",
	"rxvt-cygwin",
	"rxvt-cygwin-native",
	"rxvt-unicode",
	"screen",
	"screen-bce",
	"screen-w",
	"screen.linux",
	"vt100",
	"vt220",
	"wsvt25",
	"xterm",
	"xterm-256color",
	"xterm-color",
	"xterm-debian",
	NULL
};
#endif

/* strlcat and strlcpy are nice, shame glibc does not define them */
#ifdef __GLIBC__
#  if ! defined (__UCLIBC__) && ! defined (__dietlibc__)
static size_t strlcat (char *dst, const char *src, size_t size)
{
	char *d = dst;
	const char *s = src;
	size_t src_n = size;
	size_t dst_n;

	while (src_n-- != 0 && *d != '\0')
		d++;
	dst_n = d - dst;
	src_n = size - dst_n;

	if (src_n == 0)
		return (dst_n + strlen (src));

	while (*s != '\0') {
		if (src_n != 1) {
			*d++ = *s;
			src_n--;
		}
		s++;
	}
	*d = '\0';

	return (dst_n + (s - src));
}

static size_t strlcpy (char *dst, const char *src, size_t size)
{
	const char *s = src;
	size_t n = size;

	if (n && --n)
		do {
			if (! (*dst++ = *src++))
				break;
		} while (--n);

	if (! n) {
		if (size)
			*dst = '\0';
		while (*src++);
	}

	return (src - s - 1);
}
#  endif
#endif

static bool yesno (const char *value)
{
	if (! value) {
		errno = ENOENT;
		return (false);
	}

	if (strcasecmp (value, "yes") == 0 ||
	    strcasecmp (value, "y") == 0 ||
	    strcasecmp (value, "true") == 0 ||
	    strcasecmp (value, "on") == 0 ||
	    strcasecmp (value, "1") == 0)
		return (true);

	if (strcasecmp (value, "no") != 0 &&
	    strcasecmp (value, "n") != 0 &&
	    strcasecmp (value, "false") != 0 &&
	    strcasecmp (value, "off") != 0 &&
	    strcasecmp (value, "0") != 0)
		errno = EINVAL;

	return (false);
}

static bool noyes (const char *value)
{
	int serrno = errno;
	bool retval;

	errno = 0;
	retval = yesno (value);
	if (errno == 0) {
		retval = ! retval;
		errno = serrno;
	}

	return (retval);
}

static bool is_quiet()
{
	return (yesno (getenv ("EINFO_QUIET")));
}

static bool is_verbose()
{
	return (yesno (getenv ("EINFO_VERBOSE")));
}

/* Fake tgoto call - very crapy, but works for our needs */
#ifndef HAVE_TERMCAP
static char *tgoto (const char *cap, int a, int b)
{
	static char buf[20];

	snprintf (buf, sizeof (buf), cap, b, a);
	return (buf);
}
#endif

static bool colour_terminal (FILE * __EINFO_RESTRICT f)
{
	static int in_colour = -1;
	char *e;
	int c;
	const char *_af = NULL;
	const char *_ce = NULL;
	const char *_ch = NULL;
	const char *_md = NULL;
	const char *_me = NULL;
	const char *_up = NULL;
	char tmp[100];
	char *p;
	unsigned int i = 0;

	if (f && ! isatty (fileno (f)))
		return (false);

	if (noyes (getenv ("EINFO_COLOR")))
		return (false);

	if (in_colour == 0)
		return (false);
	if (in_colour == 1)
		return (true);

	term_is_cons25 = false;

	if (! term) {
		term = getenv ("TERM");
		if (! term)
			return (false);
	}

	if (strcmp (term, "cons25") == 0)
		term_is_cons25 = true;

#ifdef HAVE_TERMCAP
	/* Check termcap to see if we can do colour or not */
	if (tgetent (termcapbuf, term) == 1) {
		char *bp = tcapbuf;

		_af = tgetstr ("AF", &bp);
		_ce = tgetstr ("ce", &bp);
		_ch = tgetstr ("ch", &bp);
		/* Our ch use also works with RI .... for now */
		if (! _ch)
			_ch = tgetstr ("RI", &bp);
		_md = tgetstr ("md", &bp);
		_me = tgetstr ("me", &bp);
		_up = tgetstr ("up", &bp);
	}

	/* Cheat here as vanilla BSD has the whole termcap info in /usr
	 * which is not available to us when we boot */
	if (term_is_cons25 || strcmp (term, "wsvt25") == 0) {
#else
		while (color_terms[i]) {
			if (strcmp (color_terms[i], term) == 0) {
				in_colour = 1;
			}
			i++;
		}

		if (in_colour != 1) {
			in_colour = 0;
			return (false);
		}
#endif
		if (! _af)
			_af = AF;
		if (! _ce)
			_ce = CE;
		if (! _ch)
			_ch = CH;
		if (! _md)
			_md = MD;
		if (! _me)
			_me = ME;
		if (! _up)
			_up = UP;
#ifdef HAVE_TERMCAP
	}

	if (! _af || ! _ce || ! _me || !_md || ! _up) {
		in_colour = 0;
		return (false);
	}

	/* Many termcap databases don't have ch or RI even though they
	 * do work */
	if (! _ch)
		_ch = CH;
#endif

	/* Now setup our colours */
	p = ebuffer;
	for (i = 0; i < sizeof (ecolors) / sizeof (ecolors[0]); i++) {
		tmp[0] = '\0';

		if (ecolors[i].name) {
			const char *bold = _md;
			c = ecolors[i].def;

			/* See if the user wants to override the colour
			 * We use a :col;bold: format like 2;1: for bold green
			 * and 1;0: for a normal red */
			if ((e = getenv("EINFO_COLOR"))) {
				char *ee = strstr (e, ecolors[i].name);

				if (ee)
					ee += strlen (ecolors[i].name);

				if (ee && *ee == '=') {
					char *d = strdup (ee + 1);
					if (d) {
						char *end = strchr (d, ':');
						if (end)
							*end = '\0';

						c = atoi(d);

						end = strchr (d, ';');
						if (end && *++end == '0')
							bold = _me;

						free (d);
					}
				}
			}
			strlcpy (tmp, tgoto (bold, 0, 0), sizeof (tmp));
			strlcat (tmp, tgoto (_af, 0, c & 0x07), sizeof (tmp));
		} else
			_GET_CAP (tmp, _me);

		if (tmp[0])
			_ASSIGN_CAP (ecolors[i].str)
		else
			ecolors[i].str = &nullstr;
	}

	_GET_CAP (tmp, _ce)
		_ASSIGN_CAP (flush)
		_GET_CAP (tmp, _up);
	_ASSIGN_CAP (up);
	strlcpy (tmp, _ch, sizeof (tmp));
	_ASSIGN_CAP (goto_column);

	in_colour = 1;
	return (true);
}

static int get_term_columns (FILE * __EINFO_RESTRICT stream)
{
	struct winsize ws;
	char *env = getenv ("COLUMNS");
	char *p;
	int i;

	if (env) {
		i = strtol (env, &p, 10);
		if (! *p)
			return (i);
	}

	if (ioctl (fileno (stream), TIOCGWINSZ, &ws) == 0)
		return (ws.ws_col);

	return (DEFAULT_COLS);
}

void eprefix (const char *__EINFO_RESTRICT prefix)
{
	_eprefix = prefix;
}
hidden_def(eprefix)

static void elogv (int level, const char *__EINFO_RESTRICT fmt, va_list ap)
{
	char *e = getenv ("EINFO_LOG");
	va_list apc;

	if (fmt && e) {
		closelog ();
		openlog (e, LOG_PID, LOG_DAEMON);
		va_copy (apc, ap);
		vsyslog (level, fmt, apc);
		va_end (apc);
		closelog ();
	}
}

void elog (int level, const char *__EINFO_RESTRICT fmt, ...)
{
	va_list ap;

	va_start (ap, fmt);
	elogv (level, fmt, ap);
	va_end (ap);
}
hidden_def(elog)

static int _eindent (FILE * __EINFO_RESTRICT stream)
{
	char *env = getenv ("EINFO_INDENT");
	int amount = 0;
	char indent[INDENT_MAX];

	if (env) {
		errno = 0;
		amount = strtol (env, NULL, 0);
		if (errno != 0 || amount < 0)
			amount = 0;
		else if (amount > INDENT_MAX)
			amount = INDENT_MAX;

		if (amount > 0)
			memset (indent, ' ', amount);
	}

	/* Terminate it */
	memset (indent + amount, 0, 1);
	return (fprintf (stream, "%s", indent));
}

static const char *_ecolor (FILE * __EINFO_RESTRICT f, einfo_color_t color)
{
	unsigned int i;

	if (! colour_terminal (f))
		return ("");

	for (i = 0; i < sizeof (ecolors) / sizeof (ecolors[0]); i++) {
		if (ecolors[i].color == color)
			return (ecolors[i].str);
	}

	return ("");
}
hidden_def(ecolor)

const char *ecolor (einfo_color_t color)
{
	FILE *f = stdout;

	/* Try and guess a valid tty */
	if (! isatty (fileno (f))) {
		f = stderr;
		if (! isatty (fileno (f))) {
			f = stdin;
			if (! isatty (fileno (f)))
				f = NULL;
		}
	}

	return (_ecolor (f, color));
}

#define LASTCMD(_cmd) { \
	unsetenv ("EINFO_LASTCMD"); \
	setenv ("EINFO_LASTCMD", _cmd, 1); \
}

#define EINFOVN(_file, _color) \
{ \
	char *_e = getenv ("EINFO_LASTCMD"); \
	if (_e && ! colour_terminal (_file) && strcmp (_e, "ewarn") != 0 && \
	    _e[strlen (_e) - 1] == 'n') \
	fprintf (_file, "\n"); \
	if (_eprefix) \
	fprintf (_file, "%s%s%s|", _ecolor (_file, _color), _eprefix, _ecolor (_file, ECOLOR_NORMAL)); \
	fprintf (_file, " %s*%s ", _ecolor (_file, _color), _ecolor (_file, ECOLOR_NORMAL)); \
	retval += _eindent (_file); \
	{ \
		va_list _ap; \
		va_copy (_ap, ap); \
		retval += vfprintf (_file, fmt, _ap) + 3; \
		va_end (_ap); \
	} \
	if (colour_terminal (_file)) \
	fprintf (_file, "%s", flush); \
}

static int _einfovn (const char *__EINFO_RESTRICT fmt, va_list ap)
{
	int retval = 0;

	EINFOVN (stdout, ECOLOR_GOOD);
	return (retval);
}

static int _ewarnvn (const char *__EINFO_RESTRICT fmt, va_list ap)
{
	int retval = 0;

	EINFOVN (stdout, ECOLOR_WARN);
	return (retval);
}

static int _eerrorvn (const char *__EINFO_RESTRICT fmt, va_list ap)
{
	int retval = 0;

	EINFOVN (stderr, ECOLOR_BAD);
	return (retval);
}

int einfon (const char *__EINFO_RESTRICT fmt, ...)
{
	int retval;
	va_list ap;

	if (! fmt || is_quiet ())
		return (0);

	va_start (ap, fmt);
	retval = _einfovn (fmt, ap);
	va_end (ap);

	LASTCMD ("einfon");

	return (retval);
}
hidden_def(einfon)

int ewarnn (const char *__EINFO_RESTRICT fmt, ...)
{
	int retval;
	va_list ap;

	if (! fmt || is_quiet ())
		return (0);

	va_start (ap, fmt);
	retval = _ewarnvn (fmt, ap);
	va_end (ap);

	LASTCMD ("ewarnn");

	return (retval);
}
hidden_def(ewarnn)

int eerrorn (const char *__EINFO_RESTRICT fmt, ...)
{
	int retval;
	va_list ap;

	va_start (ap, fmt);
	retval = _eerrorvn (fmt, ap);
	va_end (ap);

	LASTCMD ("errorn");

	return (retval);
}
hidden_def(eerrorn)

int einfo (const char *__EINFO_RESTRICT fmt, ...)
{
	int retval;
	va_list ap;

	if (! fmt || is_quiet())
		return (0);

	va_start (ap, fmt);
	retval = _einfovn (fmt, ap);
	retval += printf ("\n");
	va_end (ap);

	LASTCMD ("einfo");

	return (retval);
}
hidden_def(einfo)

int ewarn (const char *__EINFO_RESTRICT fmt, ...)
{
	int retval;
	va_list ap;

	if (! fmt || is_quiet ())
		return (0);

	va_start (ap, fmt);
	elogv (LOG_WARNING, fmt, ap);
	retval = _ewarnvn (fmt, ap);
	retval += printf ("\n");
	va_end (ap);

	LASTCMD ("ewarn");

	return (retval);
}
hidden_def(ewarn)

void ewarnx (const char *__EINFO_RESTRICT fmt, ...)
{
	int retval;
	va_list ap;

	if (fmt && ! is_quiet ()) {
		va_start (ap, fmt);
		elogv (LOG_WARNING, fmt, ap);
		retval = _ewarnvn (fmt, ap);
		va_end (ap);
		retval += printf ("\n");
	}
	exit (EXIT_FAILURE);
}
hidden_def(ewarnx)

int eerror (const char *__EINFO_RESTRICT fmt, ...)
{
	int retval;
	va_list ap;

	if (! fmt)
		return (0);

	va_start (ap, fmt);
	elogv (LOG_ERR, fmt, ap);
	retval = _eerrorvn (fmt, ap);
	va_end (ap);
	retval += fprintf (stderr, "\n");

	LASTCMD ("eerror");

	return (retval);
}
hidden_def(eerror)

void eerrorx (const char *__EINFO_RESTRICT fmt, ...)
{
	va_list ap;

	if (fmt) {
		va_start (ap, fmt);
		elogv (LOG_ERR, fmt, ap);
		_eerrorvn (fmt, ap);
		va_end (ap);
		fprintf (stderr, "\n");
	}

	exit (EXIT_FAILURE);
}
hidden_def(eerrorx)

int ebegin (const char *__EINFO_RESTRICT fmt, ...)
{
	int retval;
	va_list ap;

	if (! fmt || is_quiet ())
		return (0);

	va_start (ap, fmt);
	retval = _einfovn (fmt, ap);
	va_end (ap);
	retval += printf (" ...");
	if (colour_terminal (stdout))
		retval += printf ("\n");

	LASTCMD ("ebegin");

	return (retval);
}
hidden_def(ebegin)

static void _eend (FILE * __EINFO_RESTRICT fp, int col, einfo_color_t color,
		   const char *msg)
{
	int i;
	int cols;

	if (! msg)
		return;

	cols = get_term_columns (fp) - (strlen (msg) + 3);

	/* cons25 is special - we need to remove one char, otherwise things
	 * do not align properly at all. */
	if (! term) {
		term = getenv ("TERM");
		if (term && strcmp (term, "cons25") == 0)
			term_is_cons25 = true;
		else
			term_is_cons25 = false;
	}
	if (term_is_cons25)
		cols--;

	if (cols > 0 && colour_terminal (fp)) {
		fprintf (fp, "%s%s %s[%s%s%s]%s\n", up, tgoto (goto_column, 0, cols),
			 ecolor (ECOLOR_BRACKET), ecolor (color), msg,
			 ecolor (ECOLOR_BRACKET), ecolor (ECOLOR_NORMAL));
	} else {
		if (col > 0)
			for (i = 0; i < cols - col; i++)
				fprintf (fp, " ");
		fprintf (fp, " [%s]\n", msg);
	}
}

static int _do_eend (const char *cmd, int retval, const char *__EINFO_RESTRICT fmt, va_list ap)
{
	int col = 0;
	FILE *fp = stdout;
	va_list apc;

	if (fmt && strlen (fmt) > 0 && retval != 0) {
		va_copy (apc, ap);
		if (strcmp (cmd, "ewend") == 0) {
			col = _ewarnvn (fmt, apc);
			fp = stdout;
		} else {
			col = _eerrorvn (fmt, apc);
			fp = stderr;
		}
		col += fprintf (fp, "\n");
		va_end (apc);
	}

	_eend (fp, col,
	       retval == 0 ? ECOLOR_GOOD : ECOLOR_BAD,
	       retval == 0 ? OK : NOT_OK);
	return (retval);
}

int eend (int retval, const char *__EINFO_RESTRICT fmt, ...)
{
	va_list ap;

	if (is_quiet ())
		return (retval);

	va_start (ap, fmt);
	_do_eend ("eend", retval, fmt, ap);
	va_end (ap);

	LASTCMD ("eend");

	return (retval);
}
hidden_def(eend)

int ewend (int retval, const char *__EINFO_RESTRICT fmt, ...)
{
	va_list ap;

	if (is_quiet ())
		return (retval);

	va_start (ap, fmt);
	_do_eend ("ewend", retval, fmt, ap);
	va_end (ap);

	LASTCMD ("ewend");

	return (retval);
}
hidden_def(ewend)

void ebracket (int col, einfo_color_t color, const char *msg)
{
	_eend (stdout, col, color, msg);
}
hidden_def(ebracket)

void eindent (void)
{
	char *env = getenv ("EINFO_INDENT");
	int amount = 0;
	char num[10];

	if (env) {
		errno = 0;
		amount = strtol (env, NULL, 0);
		if (errno != 0)
			amount = 0;
	}

	amount += INDENT_WIDTH;
	if (amount > INDENT_MAX)
		amount = INDENT_MAX;

	snprintf (num, 10, "%08d", amount);
	setenv ("EINFO_INDENT", num, 1);
}
hidden_def(eindent)

void eoutdent (void)
{
	char *env = getenv ("EINFO_INDENT");
	int amount = 0;
	char num[10];

	if (! env)
		return;

	errno = 0;
	amount = strtol (env, NULL, 0);
	if (errno != 0)
		amount = 0;
	else
		amount -= INDENT_WIDTH;

	if (amount <= 0)
		unsetenv ("EINFO_EINDENT");
	else {
		snprintf (num, 10, "%08d", amount);
		setenv ("EINFO_EINDENT", num, 1);
	}
}
hidden_def(eoutdent)

int einfovn (const char *__EINFO_RESTRICT fmt, ...)
{
	int retval;
	va_list ap;

	if (! fmt || ! is_verbose ())
		return (0);

	va_start (ap, fmt);
	retval = _einfovn (fmt, ap);
	va_end (ap);

	LASTCMD ("einfovn");

	return (retval);
}
hidden_def(einfovn)

int ewarnvn (const char *__EINFO_RESTRICT fmt, ...)
{
	int retval;
	va_list ap;

	if (! fmt || ! is_verbose ())
		return (0);

	va_start (ap, fmt);
	retval = _ewarnvn (fmt, ap);
	va_end (ap);

	LASTCMD ("ewarnvn");

	return (retval);
}
hidden_def(ewarnvn)

int einfov (const char *__EINFO_RESTRICT fmt, ...)
{
	int retval;
	va_list ap;

	if (! fmt || ! is_verbose ())
		return (0);

	va_start (ap, fmt);
	retval = _einfovn (fmt, ap);
	retval += printf ("\n");
	va_end (ap);

	LASTCMD ("einfov");

	return (retval);
}
hidden_def(einfov)

int ewarnv (const char *__EINFO_RESTRICT fmt, ...)
{
	int retval;
	va_list ap;

	if (! fmt || ! is_verbose ())
		return (0);

	va_start (ap, fmt);
	retval = _ewarnvn (fmt, ap);
	retval += printf ("\n");
	va_end (ap);

	LASTCMD ("ewarnv");

	return (retval);
}
hidden_def(ewarnv)

int ebeginv (const char *__EINFO_RESTRICT fmt, ...)
{
	int retval;
	va_list ap;

	if (! fmt || ! is_verbose ())
		return (0);

	va_start (ap, fmt);
	retval = _einfovn (fmt, ap);
	retval += printf (" ...");
	if (colour_terminal (stdout))
		retval += printf ("\n");
	va_end (ap);

	LASTCMD ("ebeginv");

	return (retval);
}
hidden_def(ebeginv)

int eendv (int retval, const char *__EINFO_RESTRICT fmt, ...)
{
	va_list ap;

	if (! is_verbose ())
		return (0);

	va_start (ap, fmt);
	_do_eend ("eendv", retval, fmt, ap);
	va_end (ap);

	LASTCMD ("eendv");

	return (retval);
}
hidden_def(eendv)

int ewendv (int retval, const char *__EINFO_RESTRICT fmt, ...)
{
	va_list ap;

	if (! is_verbose ())
		return (0);

	va_start (ap, fmt);
	_do_eend ("ewendv", retval, fmt, ap);
	va_end (ap);

	LASTCMD ("ewendv");

	return (retval);
}
hidden_def(ewendv)

void eindentv (void)
{
	if (is_verbose ())
		eindent ();
}
hidden_def(eindentv)

void eoutdentv (void)
{
	if (is_verbose ())
		eoutdent ();
}
hidden_def(eoutdentv)
