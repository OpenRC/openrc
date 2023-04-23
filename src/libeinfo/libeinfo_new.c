/*
 * TODO:
 * 	- handle the fancy env vars
 */
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>

#ifdef HAVE_CURSES
# include <curses.h>
# include <term.h>
#else
# warning "no curses found"
#endif

#include "einfo.h"

#define EINFO_NONNULL		__attribute__((__nonnull__))
#define EINFO_NONNULL_POS(X)	__attribute__((__nonnull__(X)))

/* TODO: implement for realsies */
#define LASTCMD(X)

/* Spaces per indent */
#define INDENT_WIDTH	2
/* Max indent level (in spaces) */
#define INDENT_MAX	40

struct einfo_term {
	/* Where to actualy write stuff to */
	FILE *file;
	/* Terminal used for color stuff */
	TERMINAL *term;
	/* Is the terminal ready for use */
	bool is_set_up;
};

static struct einfo_term stdout_term = {
	.file = NULL,
	.term = NULL,
	.is_set_up = false
};
static struct einfo_term stderr_term = {
	.file = NULL,
	.term = NULL,
	.is_set_up = false
};
static struct einfo_term *einfo_cur_term = NULL;

static size_t indent_level = 0;

/* Prefixed to messages as "prefix| * msg" */
static const char *prefix = NULL;

/* Set up the terminals, if needed */
static void _setupterm(void)
{
	/* TODO: check the errors from setupterm and stuff */
	if (!stdout_term.is_set_up) {
		stdout_term.file = stdout;
		setupterm(NULL, fileno(stdout), NULL);
		stdout_term.term = cur_term;
		stdout_term.is_set_up = true;
	}
	if (!stderr_term.is_set_up) {
		stderr_term.file = stderr;
		setupterm(NULL, fileno(stderr), NULL);
		stderr_term.term = cur_term;
		stderr_term.is_set_up = true;
	}
}

/*
 * Clean up the terminals. Technically not strictly needed since the memory is
 * freed up on program exit, but let's try to do it right anyway ;)
 */
__attribute__((__destructor__))
static void _cleanupterm(void)
{
	/* printf("\ndestructor called\n"); */
	if (stdout_term.is_set_up && stdout_term.term != NULL) {
		/* printf("cleaning up stdout_term\n"); */
		del_curterm(stdout_term.term);
	}
	if (stderr_term.is_set_up && stderr_term.term != NULL) {
		/* printf("cleaning up stderr_term\n"); */
		del_curterm(stderr_term.term);
	}
}

/* TODO: implement for realsies */
static bool is_quiet(void)
{
	return false;
}
static bool is_really_quiet(void)
{
	return false;
}
static bool is_verbose(void)
{
	return true;
}

/* putchar(3)-like for use with tputs */
static int _putc(int c)
{
	return fputc(c, einfo_cur_term->file);
}

/*
 * Set the output color mode
 *
 * t: the terminal to set the color for
 * color: the color to being using
 *
 * TODO:
 * 	- actual color stuff (including EINFO_COLOR)
 * 	- check errors
 * 	- proper string concatenation
 */
EINFO_NONNULL
static void _ecolor(struct einfo_term *t, ECOLOR color)
{
	char color_str [100] = { 0 };

	_setupterm();
	set_curterm(t->term);
	einfo_cur_term = t;

	if (color != ECOLOR_NORMAL) {
		strcat(color_str, tiparm(enter_bold_mode));
	} else {
		strcat(color_str, tiparm(exit_attribute_mode));
	}

	if (color == ECOLOR_GOOD) {
		strcat(color_str, tiparm(set_a_foreground, COLOR_GREEN));
	} else if (color == ECOLOR_WARN) {
		strcat(color_str, tiparm(set_a_foreground, COLOR_YELLOW));
	} else if (color == ECOLOR_BAD) {
		strcat(color_str, tiparm(set_a_foreground, COLOR_RED));
	} else if (color == ECOLOR_HILITE) {
		strcat(color_str, tiparm(set_a_foreground, COLOR_CYAN));
	} else if (color == ECOLOR_BRACKET) {
		strcat(color_str, tiparm(set_a_foreground, COLOR_BLUE));
	}
	/* ECOLOR_NORMAL */
	else {
		strcat(color_str, tiparm(orig_pair));
	}

	if (color_str[0] != '\0') {
		tputs(color_str, 1, _putc);
	}
}

/*
 * Move up one row
 *
 * t: terminal to move in
 *
 * TODO:
 * 	- check errors
 */
EINFO_NONNULL
static void _move_up(struct einfo_term *t)
{
	const char *move_str = "";

	_setupterm();
	set_curterm(t->term);
	einfo_cur_term = t;

	move_str = tiparm(cursor_up);
	tputs(move_str, 1, _putc);
}

/*
 * Move to a specified column
 *
 * t: terminal to move in
 * col: column to move to
 *      >0 is counting from the left edge
 *      <0 is counting from the right edge
 *      values that would "overflow" get chomped
 *
 * TODO:
 * 	- check errors
 * 	- chomp
 */
EINFO_NONNULL
static void _move_col(struct einfo_term *t, int col)
{
	const char *move_str = "";

	_setupterm();
	set_curterm(t->term);
	einfo_cur_term = t;

	if (col == 0) {
		return;
	} else if (col < 0) {
		col += columns;
	}

	move_str = tiparm(column_address, col);
	tputs(move_str, 1, _putc);
}

/*
 * Perform the indent, if needed
 *
 * f: the file that is indented into
 *
 * return: like fprintf(3)
 *
 * TODO:
 * 	- read env value?
 * 	- use cursor controls?
 */
EINFO_NONNULL
static int _eindent(FILE *f)
{
	/* Pre-terminated :) */
	char indent [INDENT_MAX + 1] = { 0 };

	/*
	 * The bounds are checked in eindent()/eoutdent(), this is just to skip
	 * unneeded work. Will need to be re-checked once reading EINFO_INDENT
	 * env var is implemented.
	 */
	if (indent_level > 0) {
		memset(indent, ' ', indent_level * INDENT_WIDTH);
		return fprintf(f, "%s", indent);
	}

	return 0;
}

/*
 * Actually print the einfo message
 *
 * t: the terminal to print the message to
 * color: the color to use for the einfo marker
 * fmt: the printf-style format for the message
 * va: the args to feed vfprintf(3)
 *
 * return: like the printf-family of functions
 *
 * TODO:
 * 	- handle the EINFO_LASTCMD stuff
 */
EINFO_NONNULL
EINFO_PRINTF(3, 0)
static int _einfo(
	struct einfo_term *t,
	ECOLOR color,
	const char *EINFO_RESTRICT fmt,
	va_list va)
{
	int ret = 0;
	va_list ap;

	va_copy(ap, va);

	if (prefix) {
		_ecolor(t, color);
		ret += fprintf(t->file, "%s", prefix);
		_ecolor(t, ECOLOR_NORMAL);
		ret += fprintf(t->file, "|");
	}
	_ecolor(t, color);
	ret += fprintf(t->file, " * ");
	_ecolor(t, ECOLOR_NORMAL);
	ret += _eindent(t->file);
	ret += vfprintf(t->file, fmt, ap);

	va_end(ap);
	return ret;
}

/*
 * Print the first part of the eend if applicable
 *
 * args: similar to _einfo()
 * fmt: potentially NULL or empty
 *
 * return: similar to _einfo()
 */
EINFO_NONNULL_POS(1)
EINFO_PRINTF(4, 0)
static int _eend_message(
	struct einfo_term *t,
	int retval,
	ECOLOR color,
	const char *EINFO_RESTRICT fmt,
	va_list va)
{
	int ret;
	va_list ap;

	/* Nothing to see here */
	if (retval == 0 || fmt == NULL || fmt[0] == '\0') {
		return 0;
	}

	va_copy(ap, va);

	ret = _einfo(t, color, fmt, ap);
	ret += fprintf(t->file, "\n");

	va_end(ap);
	return ret;
}

/*
 * Print the status part of eend
 *
 * t: the terminal to print to
 * col: the column where the status begins, measured from the initial ' '
 *      -1 to align at the right edge of the terminal
 * color: the color to print with
 * msg: message to include in the brackets
 *
 * TODO:
 * 	- cursor placement
 * 	- do something different on empty message
 */
EINFO_NONNULL
static int _eend_status(
	struct einfo_term *t,
	int col,
	ECOLOR color,
	const char *EINFO_RESTRICT msg)
{
	int ret;

	if (msg[0] == '\0') {
		return 0;
	}

	if (col == -1) {
		col = -(strlen(msg) + 5);
	}

	_move_up(t);
	_move_col(t, col);
	_ecolor(t, ECOLOR_BRACKET);
	ret = fprintf(t->file, " [ ");
	_ecolor(t, color);
	ret += fprintf(t->file, "%s", msg);
	_ecolor(t, ECOLOR_BRACKET);
	ret += fprintf(t->file, " ]\n");
	_ecolor(t, ECOLOR_NORMAL);

	return ret;
}

/*
 * Print an einfo message to syslog (without the " * " or terminators). Requires
 * an ID to be given in EINFO_LOG env var.
 *
 * level: log level, see syslog(3)
 * fmt: the printf-style format for the message
 * va: the args to feed vsyslog(3)
 *
 * TODO:
 * 	- get the log id from the env
 * 	- can this be nonnull?
 */
EINFO_PRINTF(2, 0)
static void _elog(int level, const char *EINFO_RESTRICT fmt, va_list va)
{
	const char *id = "elog";

	va_list ap;

	if (fmt && id) {
		va_copy(ap, va);

		openlog(id, LOG_PID, LOG_DAEMON);
		vsyslog(level, fmt, ap);
		/* Only Linux seems to mention closelog(3) being optional */
		closelog();

		va_end(ap);
	}
}

const char *ecolor(ECOLOR color)
{
	(void)color;
	return "";
}

void elog(int level, const char * EINFO_RESTRICT fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);

	_elog(level, fmt, ap);

	va_end(ap);
}

int einfon(const char * EINFO_RESTRICT fmt, ...)
{
	int ret;
	va_list ap;

	if (!fmt || is_quiet()) {
		return 0;
	}

	va_start(ap, fmt);

	ret = _einfo(&stdout_term, ECOLOR_GOOD, fmt, ap);

	va_end(ap);
	LASTCMD(__func__);
	return ret;
}
int ewarnn(const char * EINFO_RESTRICT fmt, ...)
{
	int ret;
	va_list ap;

	if (!fmt || is_quiet()) {
		return 0;
	}

	va_start(ap, fmt);

	ret = _einfo(&stderr_term, ECOLOR_WARN, fmt, ap);

	va_end(ap);
	LASTCMD(__func__);
	return ret;
}
int eerrorn(const char * EINFO_RESTRICT fmt, ...)
{
	int ret;
	va_list ap;

	if (!fmt || is_really_quiet()) {
		return 0;
	}

	va_start(ap, fmt);

	ret = _einfo(&stderr_term, ECOLOR_BAD, fmt, ap);

	va_end(ap);
	LASTCMD(__func__);
	return ret;
}
int einfo(const char * EINFO_RESTRICT fmt, ...)
{
	int ret;
	va_list ap;

	if (!fmt || is_quiet()) {
		return 0;
	}

	va_start(ap, fmt);

	ret = _einfo(&stdout_term, ECOLOR_GOOD, fmt, ap);
	ret += fprintf(stdout_term.file, "\n");

	va_end(ap);
	LASTCMD(__func__);
	return ret;
}
/* TODO: apparently this is supposed to go to syslog too? */
int ewarn(const char * EINFO_RESTRICT fmt, ...)
{
	int ret;
	va_list ap;

	if (!fmt || is_quiet()) {
		return 0;
	}

	va_start(ap, fmt);

	ret = _einfo(&stderr_term, ECOLOR_WARN, fmt, ap);
	ret += fprintf(stderr_term.file, "\n");

	va_end(ap);
	LASTCMD(__func__);
	return ret;
}
/* TODO: apparently this is supposed to go to syslog too? */
void ewarnx(const char * EINFO_RESTRICT fmt, ...)
{
	va_list ap;

	if (fmt && !is_quiet()) {
		va_start(ap, fmt);

		_einfo(&stderr_term, ECOLOR_WARN, fmt, ap);
		fprintf(stderr_term.file, "\n");

		va_end(ap);
	}

	exit(EXIT_FAILURE);
}
/* TODO: apparently this is supposed to go to syslog too? */
int eerror(const char * EINFO_RESTRICT fmt, ...)
{
	int ret;
	va_list ap;

	if (!fmt || is_really_quiet()) {
		return 0;
	}

	va_start(ap, fmt);

	ret = _einfo(&stderr_term, ECOLOR_BAD, fmt, ap);
	ret += fprintf(stderr_term.file, "\n");

	va_end(ap);
	LASTCMD(__func__);
	return ret;
}
/* TODO: apparently this is supposed to go to syslog too? */
void eerrorx(const char * EINFO_RESTRICT fmt, ...)
{
	va_list ap;

	if (fmt && !is_really_quiet()) {
		va_start(ap, fmt);

		_einfo(&stderr_term, ECOLOR_BAD, fmt, ap);
		fprintf(stderr_term.file, "\n");

		va_end(ap);
	}

	exit(EXIT_FAILURE);
}

int einfovn(const char * EINFO_RESTRICT fmt, ...)
{
	int ret;
	va_list ap;

	if (!fmt || !is_verbose()) {
		return 0;
	}

	va_start(ap, fmt);

	ret = _einfo(&stdout_term, ECOLOR_GOOD, fmt, ap);

	va_end(ap);
	LASTCMD(__func__);
	return ret;
}
int ewarnvn(const char * EINFO_RESTRICT fmt, ...)
{
	int ret;
	va_list ap;

	if (!fmt || !is_verbose()) {
		return 0;
	}

	va_start(ap, fmt);

	ret = _einfo(&stderr_term, ECOLOR_WARN, fmt, ap);

	va_end(ap);
	LASTCMD(__func__);
	return ret;
}
/* Not implemented originally? */
int ebeginvn(const char * EINFO_RESTRICT fmt, ...)
{
	(void)fmt;
	return 0;
}
int eendvn(int n, const char * EINFO_RESTRICT fmt, ...)
{
	(void)n;
	(void)fmt;
	return 0;
}
int ewendvn(int n, const char * EINFO_RESTRICT fmt, ...)
{
	(void)n;
	(void)fmt;
	return 0;
}
/* End not implemented */
int einfov(const char * EINFO_RESTRICT fmt, ...)
{
	int ret;
	va_list ap;

	if (!fmt || !is_verbose()) {
		return 0;
	}

	va_start(ap, fmt);

	ret = _einfo(&stdout_term, ECOLOR_GOOD, fmt, ap);
	ret += fprintf(stdout_term.file, "\n");

	va_end(ap);
	LASTCMD(__func__);
	return ret;
}
int ewarnv(const char * EINFO_RESTRICT fmt, ...)
{
	int ret;
	va_list ap;

	if (!fmt || !is_verbose()) {
		return 0;
	}

	va_start(ap, fmt);

	ret = _einfo(&stderr_term, ECOLOR_WARN, fmt, ap);
	ret += fprintf(stderr_term.file, "\n");

	va_end(ap);
	LASTCMD(__func__);
	return ret;
}

int ebeginv(const char * EINFO_RESTRICT fmt, ...)
{
	int ret;
	va_list ap;

	if (!fmt || !is_verbose()) {
		return 0;
	}

	va_start(ap, fmt);

	ret = _einfo(&stdout_term, ECOLOR_GOOD, fmt, ap);
	ret += fprintf(stdout_term.file, " ...\n");

	va_end(ap);
	LASTCMD(__func__);
	return ret;
}
int ebegin(const char * EINFO_RESTRICT fmt, ...)
{
	int ret;
	va_list ap;

	if (!fmt || is_quiet()) {
		return 0;
	}

	va_start(ap, fmt);

	ret = _einfo(&stdout_term, ECOLOR_GOOD, fmt, ap);
	ret += fprintf(stdout_term.file, " ...\n");

	va_end(ap);
	LASTCMD(__func__);
	return ret;
}

int eend(int retval, const char * EINFO_RESTRICT fmt, ...)
{
	va_list ap;
	struct einfo_term *term;
	ECOLOR color;
	const char *msg;

	if (is_quiet()) {
		return retval;
	}

	va_start(ap, fmt);

	if (retval == 0) {
		term = &stdout_term;
		color = ECOLOR_GOOD;
		msg = "ok";
	} else {
		term = &stderr_term;
		color = ECOLOR_BAD;
		msg = "!!";
	}
	_eend_message(term, retval, color, fmt, ap);
	_eend_status(term, -1, color, msg);

	va_end(ap);
	LASTCMD(__func__);
	return retval;
}
int ewend(int retval, const char * EINFO_RESTRICT fmt, ...)
{
	va_list ap;
	struct einfo_term *term;
	ECOLOR color;
	const char *msg;

	if (is_quiet()) {
		return retval;
	}

	va_start(ap, fmt);

	if (retval == 0) {
		term = &stdout_term;
		color = ECOLOR_GOOD;
		msg = "ok";
	} else {
		term = &stderr_term;
		color = ECOLOR_WARN;
		msg = "!!";
	}
	_eend_message(term, retval, color, fmt, ap);

	/* Do not use the warn color for "!!" */
	if (retval != 0) {
		color = ECOLOR_BAD;
	}
	_eend_status(term, -1, color, msg);

	va_end(ap);
	LASTCMD(__func__);
	return retval;
}
void ebracket(int col, ECOLOR color, const char * EINFO_RESTRICT msg)
{
	if (msg == NULL) {
		msg = "";
	}

	_eend_status(&stdout_term, col, color, msg);

	LASTCMD(__func__);
}

int eendv(int retval, const char * EINFO_RESTRICT fmt, ...)
{
	va_list ap;
	struct einfo_term *term;
	ECOLOR color;
	const char *msg;

	if (!is_verbose()) {
		return retval;
	}

	va_start(ap, fmt);

	if (retval == 0) {
		term = &stdout_term;
		color = ECOLOR_GOOD;
		msg = "ok";
	} else {
		term = &stderr_term;
		color = ECOLOR_BAD;
		msg = "!!";
	}
	_eend_message(term, retval, color, fmt, ap);
	_eend_status(term, -1, color, msg);

	va_end(ap);
	LASTCMD(__func__);
	return retval;
}
int ewendv(int retval, const char * EINFO_RESTRICT fmt, ...)
{
	va_list ap;
	struct einfo_term *term;
	ECOLOR color;
	const char *msg;

	if (!is_verbose()) {
		return retval;
	}

	va_start(ap, fmt);

	if (retval == 0) {
		term = &stdout_term;
		color = ECOLOR_GOOD;
		msg = "ok";
	} else {
		term = &stderr_term;
		color = ECOLOR_WARN;
		msg = "!!";
	}
	_eend_message(term, retval, color, fmt, ap);

	/* Do not use the warn color for "!!" */
	if (retval != 0) {
		color = ECOLOR_BAD;
	}
	_eend_status(term, -1, color, msg);

	va_end(ap);
	LASTCMD(__func__);
	return retval;
}

/*
 * TODO:
 * 	- proper test progs
 * 	- env shit (only read, no point in setting it?)
 */
void eindent(void)
{
	size_t new_level = indent_level + 1;

	if (new_level * INDENT_WIDTH <= INDENT_MAX) {
		indent_level = new_level;
	}
}
void eoutdent(void)
{
	if (indent_level > 0) {
		indent_level--;
	}
}
void eindentv(void)
{
	if (is_verbose()) {
		eindent();
	}
}
void eoutdentv(void)
{
	if (is_verbose()) {
		eoutdent();
	}
}

void eprefix(const char * EINFO_RESTRICT p)
{
	prefix = p;
}

/* TODO: remove */
/* vim: set tabstop=8 noexpandtab: */
