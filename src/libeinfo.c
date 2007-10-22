/*
   einfo.c
   Gentoo informational functions
   Copyright 2007 Gentoo Foundation
   */

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
#include <unistd.h>

#include "einfo.h"
#include "rc.h"
#include "rc-misc.h"

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

#define OK					"ok"
#define NOT_OK				"!!"

#define CHECK_VERBOSE		if (! is_env ("RC_VERBOSE", "yes")) return 0

    /* Number of spaces for an indent */
#define INDENT_WIDTH		2

    /* How wide can the indent go? */
#define INDENT_MAX			40

/* Default colours */
#define ECOLOR_GOOD_A      "\033[32;01m"
#define ECOLOR_WARN_A      "\033[33;01m"
#define ECOLOR_BAD_A       "\033[31;01m"
#define ECOLOR_HILITE_A    "\033[36;01m"
#define ECOLOR_BRACKET_A   "\033[34;01m"
#define ECOLOR_NORMAL_A    "\033[0m"
#define ECOLOR_FLUSH_A     "\033[K"

/* A cheat sheet of colour capable terminals
   This is taken from DIR_COLORS from GNU coreutils
   We embed it here as we shouldn't depend on coreutils */
static const char *color_terms[] = {
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
	"xterm",
	"xterm-256color",
	"xterm-color",
	"xterm-debian",
	NULL
};

static const char *term = NULL;
static bool term_is_cons25 = false;

/* A pointer to a string to prefix to einfo/ewarn/eerror messages */
static const char *_eprefix = NULL;

static bool is_env (const char *var, const char *val)
{
	char *v;

	if (! var)
		return (false);

	v = getenv (var);
	if (! v)
		return (val ? false : true);

	return (strcasecmp (v, val) ? false : true);
}

static bool colour_terminal (FILE *f)
{
	static int in_colour = -1;
	int i = 0;

	if (f && ! isatty (fileno (f)))
		return (false);

	if (is_env ("RC_NOCOLOR", "yes"))
		return (false);

	if (in_colour == 0)
		return (false);
	if (in_colour == 1)
		return (true);

	if (! term) {
		term = getenv ("TERM");
		if (! term)
			return (false);
	}

	if (strcmp (term, "cons25") == 0)
		term_is_cons25 = true;
	else
		term_is_cons25 = false;

	while (color_terms[i]) {
		if (strcmp (color_terms[i], term) == 0) {
			in_colour = 1;
			return (true);
		}
		i++;
	}

	in_colour = 0;
	return (false);
}

static int get_term_columns (FILE *stream)
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

void eprefix (const char *prefix)
{
	_eprefix = prefix;
}
hidden_def(eprefix)

static void elogv (int level, const char *fmt, va_list ap)
{
	char *e = getenv ("RC_ELOG");
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

void elog (int level, const char *fmt, ...)
{
	va_list ap;

	va_start (ap, fmt);
	elogv (level, fmt, ap);
	va_end (ap);
}
hidden_def(elog)

static int _eindent (FILE *stream)
{
	char *env = getenv ("RC_EINDENT");
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

static const char *_ecolor (FILE *f, einfo_color_t color)
{
	const char *col = NULL;

	if (! colour_terminal (f))
		return ("");
	
	switch (color) {
		case ECOLOR_GOOD:
			if (! (col = getenv ("ECOLOR_GOOD")))
				col = ECOLOR_GOOD_A;
			break;
		case ECOLOR_WARN:
			if (! (col = getenv ("ECOLOR_WARN")))
				col = ECOLOR_WARN_A;
			break;
		case ECOLOR_BAD:
			if (! (col = getenv ("ECOLOR_BAD")))
				col = ECOLOR_BAD_A;
			break;
		case ECOLOR_HILITE:
			if (! (col = getenv ("ECOLOR_HILITE")))
				col = ECOLOR_HILITE_A;
			break;
		case ECOLOR_BRACKET:
			if (! (col = getenv ("ECOLOR_BRACKET")))
				col = ECOLOR_BRACKET_A;
			break;
		case ECOLOR_NORMAL:
			col = ECOLOR_NORMAL_A;
			break;
	}

	return (col);
}
hidden_def(ecolor)

const char *ecolor (einfo_color_t color)
{
	return (_ecolor (stdout, color));
}

#define EINFOVN(_file, _color) \
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
	fprintf (_file, ECOLOR_FLUSH_A);

static int _einfovn (const char *fmt, va_list ap)
{
	int retval = 0;

	EINFOVN (stdout, ECOLOR_GOOD);
	return (retval);
}

static int _ewarnvn (const char *fmt, va_list ap)
{
	int retval = 0;

	EINFOVN (stdout, ECOLOR_WARN);
	return (retval);
}

static int _eerrorvn (const char *fmt, va_list ap)
{
	int retval = 0;

	EINFOVN (stderr, ECOLOR_BAD);
	return (retval);
}

int einfon (const char *fmt, ...)
{
	int retval;
	va_list ap;

	if (! fmt || is_env ("RC_QUIET", "yes"))
		return (0);

	va_start (ap, fmt);
	retval = _einfovn (fmt, ap);
	va_end (ap);

	return (retval);
}
hidden_def(einfon)

int ewarnn (const char *fmt, ...)
{
	int retval;
	va_list ap;

	if (! fmt || is_env ("RC_QUIET", "yes"))
		return (0);

	va_start (ap, fmt);
	retval = _ewarnvn (fmt, ap);
	va_end (ap);

	return (retval);
}
hidden_def(ewarnn)

int eerrorn (const char *fmt, ...)
{
	int retval;
	va_list ap;

	va_start (ap, fmt);
	retval = _eerrorvn (fmt, ap);
	va_end (ap);

	return (retval);
}
hidden_def(eerrorn)

int einfo (const char *fmt, ...)
{
	int retval;
	va_list ap;

	if (! fmt || is_env ("RC_QUIET", "yes"))
		return (0);

	va_start (ap, fmt);
	retval = _einfovn (fmt, ap);
	retval += printf ("\n");
	va_end (ap);

	return (retval);
}
hidden_def(einfo)

int ewarn (const char *fmt, ...)
{
	int retval;
	va_list ap;

	if (! fmt || is_env ("RC_QUIET", "yes"))
		return (0);

	va_start (ap, fmt);
	elogv (LOG_WARNING, fmt, ap);
	retval = _ewarnvn (fmt, ap);
	retval += printf ("\n");
	va_end (ap);

	return (retval);
}
hidden_def(ewarn)

void ewarnx (const char *fmt, ...)
{
	int retval;
	va_list ap;

	if (fmt && ! is_env ("RC_QUIET", "yes")) {
		va_start (ap, fmt);
		elogv (LOG_WARNING, fmt, ap);
		retval = _ewarnvn (fmt, ap);
		va_end (ap);
		retval += printf ("\n");
	}
	exit (EXIT_FAILURE);
}
hidden_def(ewarnx)

int eerror (const char *fmt, ...)
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

	return (retval);
}
hidden_def(eerror)

void eerrorx (const char *fmt, ...)
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

int ebegin (const char *fmt, ...)
{
	int retval;
	va_list ap;

	if (! fmt || is_env ("RC_QUIET", "yes"))
		return (0);

	va_start (ap, fmt);
	retval = _einfovn (fmt, ap);
	va_end (ap);
	retval += printf (" ...");
	if (colour_terminal (stdout))
		retval += printf ("\n");

	return (retval);
}
hidden_def(ebegin)

static void _eend (FILE *fp, int col, einfo_color_t color, const char *msg)
{
	int i;
	int cols;

	if (! msg)
		return;

	cols = get_term_columns (fp) - (strlen (msg) + 5);

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
		fprintf (fp, "\033[A\033[%dC %s[ %s%s %s]%s\n", cols,
				 ecolor (ECOLOR_BRACKET), ecolor (color), msg,
				 ecolor (ECOLOR_BRACKET), ecolor (ECOLOR_NORMAL));
	} else {
		if (col > 0)
			for (i = 0; i < cols - col; i++)
				fprintf (fp, " ");
		fprintf (fp, " [ %s ]\n", msg);
	}
}

static int _do_eend (const char *cmd, int retval, const char *fmt, va_list ap)
{
	int col = 0;
	FILE *fp = stdout;
	va_list apc;

	if (fmt && retval != 0)	{
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

int eend (int retval, const char *fmt, ...)
{
	va_list ap;

	if (is_env ("RC_QUIET", "yes"))
		return (retval);

	va_start (ap, fmt);
	_do_eend ("eend", retval, fmt, ap);
	va_end (ap);

	return (retval);
}
hidden_def(eend)

int ewend (int retval, const char *fmt, ...)
{
	va_list ap;

	if (is_env ("RC_QUIET", "yes"))
		return (retval);

	va_start (ap, fmt);
	_do_eend ("ewend", retval, fmt, ap);
	va_end (ap);

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
	char *env = getenv ("RC_EINDENT");
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
	setenv ("RC_EINDENT", num, 1);
}
hidden_def(eindent)

void eoutdent (void)
{
	char *env = getenv ("RC_EINDENT");
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
		unsetenv ("RC_EINDENT");
	else {
		snprintf (num, 10, "%08d", amount);
		setenv ("RC_EINDENT", num, 1);
	}
}
hidden_def(eoutdent)

int einfovn (const char *fmt, ...)
{
	int retval;
	va_list ap;

	CHECK_VERBOSE;

	if (! fmt)
		return (0);

	va_start (ap, fmt);
	retval = _einfovn (fmt, ap);
	va_end (ap);

	return (retval);
}
hidden_def(einfovn)

int ewarnvn (const char *fmt, ...)
{
	int retval;
	va_list ap;

	CHECK_VERBOSE;

	if (! fmt)
		return (0);

	va_start (ap, fmt);
	retval = _ewarnvn (fmt, ap);
	va_end (ap);

	return (retval);
}
hidden_def(ewarnvn)

int einfov (const char *fmt, ...)
{
	int retval;
	va_list ap;

	CHECK_VERBOSE;

	if (! fmt)
		return (0);

	va_start (ap, fmt);
	retval = _einfovn (fmt, ap);
	retval += printf ("\n");
	va_end (ap);

	return (retval);
}
hidden_def(einfov)

int ewarnv (const char *fmt, ...)
{
	int retval;
	va_list ap;

	CHECK_VERBOSE;

	if (! fmt)
		return (0);

	va_start (ap, fmt);
	retval = _ewarnvn (fmt, ap);
	retval += printf ("\n");
	va_end (ap);

	return (retval);
}
hidden_def(ewarnv)

int ebeginv (const char *fmt, ...)
{
	int retval;
	va_list ap;

	CHECK_VERBOSE;

	if (! fmt)
		return (0);

	va_start (ap, fmt);
	retval = _einfovn (fmt, ap);
	retval += printf (" ...");
	if (colour_terminal (stdout))
		retval += printf ("\n");
	va_end (ap);

	return (retval);
}
hidden_def(ebeginv)

int eendv (int retval, const char *fmt, ...)
{
	va_list ap;

	CHECK_VERBOSE;

	va_start (ap, fmt);
	_do_eend ("eendv", retval, fmt, ap);
	va_end (ap);

	return (retval);
}
hidden_def(eendv)

int ewendv (int retval, const char *fmt, ...)
{
	va_list ap;

	CHECK_VERBOSE;

	va_start (ap, fmt);
	_do_eend ("ewendv", retval, fmt, ap);
	va_end (ap);

	return (retval);
}
hidden_def(ewendv)

void eindentv (void)
{
	if (is_env ("RC_VERBOSE", "yes"))
		eindent ();
}
hidden_def(eindentv)

void eoutdentv (void)
{
	if (is_env ("RC_VERBOSE", "yes"))
		eoutdent ();
}
hidden_def(eoutdentv)
