/*
 * TODO:
 * 	- code style (eg, wrapped lines to spaces instead of tabs)
 */
#include <errno.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h> /* for strcasecmp(3) */
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

#define LASTCMD_ENV	"EINFO_LASTCMD"
/*
 * As soon as the last command run is set from within our code, the env var
 * becomes useless since it's no longer consistent.
 */
#define LASTCMD(X)	do {	\
	last = (X);		\
	unsetenv(LASTCMD_ENV);	\
} while (false)

/* Spaces per indent */
#define INDENT_WIDTH	2
/* Max indent level (in spaces) */
#define INDENT_MAX	40

struct einfo_term {
	/* Where to actualy write stuff to */
	FILE *file;
	/* Terminal used for color stuff */
	TERMINAL *term;
	/* Is the terminal good for curses use */
	bool is_good;
	/* Is the terminal ready for use */
	bool is_set_up;
};

/* Used to pick the final color for output */
struct color_map {
	int good;
	bool good_bold;
	int warn;
	bool warn_bold;
	int bad;
	bool bad_bold;
	int hilite;
	bool hilite_bold;
	int bracket;
	bool bracket_bold;
};

static struct einfo_term stdout_term = {
	.file = NULL,
	.term = NULL,
	.is_good = false,
	.is_set_up = false
};
static struct einfo_term stderr_term = {
	.file = NULL,
	.term = NULL,
	.is_good = false,
	.is_set_up = false
};
static struct einfo_term *einfo_cur_term = NULL;

static size_t indent_level = 0;

/* Track the last command run */
static const char *last = NULL;
/* Prefixed to messages as "prefix| * msg" */
static const char *prefix = NULL;

/* Set up the terminals, if needed */
static void _setupterm(void)
{
	/*
	 * This var is needed for setupterm() to return an error instead of
	 * exiting with an error
	 */
	int err;

	if (!stdout_term.is_set_up) {
		stdout_term.file = stdout;
		if (setupterm(NULL, fileno(stdout), &err) != ERR) {
			stdout_term.term = cur_term;
			stdout_term.is_good = true;
		}
		stdout_term.is_set_up = true;
	}
	if (!stderr_term.is_set_up) {
		stderr_term.file = stderr;
		if (setupterm(NULL, fileno(stderr), &err) != ERR) {
			stderr_term.term = cur_term;
			stderr_term.is_good = true;
		}
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

/*
 * Combination of setupterm() and set_curterm(). MUST be called at the start of
 * a function before any fancy curses stuff is done in order to allow an early
 * exit if needed.
 *
 * t: the target terminal
 *
 * return: true if the target terminal is ready for fancy stuff
 *         false otherwise
 */
EINFO_NONNULL
static bool prepare_term(struct einfo_term *t)
{
	_setupterm();
	if (t->is_good) {
		set_curterm(t->term);
	}
	einfo_cur_term = t;

	return t->is_good;
}

/*
 * Checks the truthyness of a string.
 *
 * yes/y/true/on/1
 * no/n/false/off/0
 *
 * NULL is false + ENOENT
 * other strings are false + EINVAL
 */
static bool yesno(const char *str)
{
	/* Ensure a "true" false does not have a bad errno value */
	errno = 0;

	if (str == NULL) {
		errno = ENOENT;
		return false;
	}

	if (strcasecmp(str, "yes") == 0
		|| strcasecmp(str, "y") == 0
		|| strcasecmp(str, "true") == 0
		|| strcasecmp(str, "on") == 0
		|| strcasecmp(str, "1") == 0) {
		return true;
	}

	if (strcasecmp(str, "no") != 0
		&& strcasecmp(str, "n") != 0
		&& strcasecmp(str, "false") != 0
		&& strcasecmp(str, "off") != 0
		&& strcasecmp(str, "0") != 0) {
		errno = EINVAL;
	}

	return false;
}

static bool is_quiet(void)
{
	return yesno(getenv("EINFO_QUIET"));
}
static bool is_really_quiet(void)
{
	return yesno(getenv("EERROR_QUIET"));
}
static bool is_verbose(void)
{
	return yesno(getenv("EINFO_VERBOSE"));
}

/* putchar(3)-like for use with tputs */
static int _putc(int c)
{
	return fputc(c, einfo_cur_term->file);
}

/*
 * Like strtoul(3), but with a maximum returned value. Invalid strings are
 * silently turned into 0.
 *
 * str: string to intify (must be checked by caller for NULL)
 * max: values larger than this get chomped (including overflow)
 *
 * return: the value of str as ulong, or max
 *         set errno to EINVAL on invalid strings
 *         set errno to ERANGE if max < str <= ULONG_MAX (no overflow)
 */
EINFO_NONNULL
static unsigned long chomp_strtoul(const char *str, unsigned long max)
{
	unsigned long ret = 0;
	/* Mildly gross, but not truly used while uninitialized */
	char *endptr;

	errno = 0;
	ret = strtoul(str, &endptr, 0);

	/* No valid digits */
	if (endptr == str) {
		ret = 0;
		errno = EINVAL;
	}
	/* String not completely valid */
	else if (str[0] != '\0' && endptr[0] != '\0') {
		ret = 0;
		errno = EINVAL;
	}
	/* ulong overflow */
	else if (errno == ERANGE) {
		ret = max;
	}
	else if (ret > max) {
		ret = max;
		errno = ERANGE;
	}

	return ret;
}

/*
 * Translate EINFO_COLOR env var into a set of colors. Assumes defaults unless
 * otherwise specified.
 *
 * The colors are stored in the env var as "name=color;bold:", for example:
 *
 *     EINFO_COLOR="good=1;0:warn=6;1:"
 *
 * which sets "good" messages to non-bold red and "warn" messages to bold cyan
 * assuming 1 and 6 correspond with red and cyan in curses.h, respectively.
 *
 * env: the env var itself, must be copied because getenv(3) explicitly warns
 *      about not modifying the string
 *
 * return: the (possibly) modified set of colors
 *
 * TODO:
 * 	- support named colors? (green, green-bold)
 */
static struct color_map env_colors(const char *env)
{
	char *color_env;
	char *env_saveptr = NULL;
	char *entry;
	char *entry_saveptr = NULL;
	char *name;
	char *color;
	unsigned long parsed_color;
	char *bold;
	unsigned long parsed_bold;
	struct color_map colors = {
		.good		= COLOR_GREEN,
		.good_bold	= true,
		.warn		= COLOR_YELLOW,
		.warn_bold	= true,
		.bad		= COLOR_RED,
		.bad_bold	= true,
		.hilite		= COLOR_CYAN,
		.hilite_bold	= true,
		.bracket	= COLOR_BLUE,
		.bracket_bold	= true,
	};
	int xlat_color [] = {
		/* I do not expect many people to use black tho */
		COLOR_BLACK,
		COLOR_RED,
		COLOR_GREEN,
		COLOR_YELLOW,
		COLOR_BLUE,
		COLOR_MAGENTA,
		COLOR_CYAN,
		COLOR_WHITE,
	};

	/* No env var, "", and "yes" all mean default colors */
	if (env == NULL || env[0] == '\0' || yesno(env)) {
		return colors;
	}

	color_env = strdup(env);

	/* Go through each entry in the env var, skip malformed ones */
	for (entry = strtok_r(color_env, ":", &env_saveptr);
		entry != NULL;
		entry = strtok_r(NULL, ":", &env_saveptr)) {
		/*
		 * The saveptr used for parsing an entry should be reset, see
		 * NOTES in strtok_r(3)
		 */
		entry_saveptr = NULL;

		name = strtok_r(entry, "=", &entry_saveptr);
		if (name == NULL) {
			continue;
		}
		color = strtok_r(NULL, ";", &entry_saveptr);
		if (color == NULL) {
			continue;
		}
		bold = strtok_r(NULL, ";", &entry_saveptr);
		if (bold == NULL) {
			continue;
		}
		/* Only 8 colors are defined by curses */
		parsed_color = chomp_strtoul(color, 7);
		if (errno != 0) {
			continue;
		}
		parsed_color = xlat_color[parsed_color];
		parsed_bold = chomp_strtoul(bold, 1);
		if (errno != 0) {
			continue;
		}

		if (strcasecmp(name, "good") == 0) {
			colors.good = parsed_color;
			colors.good_bold = parsed_bold;
		} else if (strcasecmp(name, "warn") == 0) {
			colors.warn = parsed_color;
			colors.warn_bold = parsed_bold;
		} else if (strcasecmp(name, "bad") == 0) {
			colors.bad = parsed_color;
			colors.bad_bold = parsed_bold;
		} else if (strcasecmp(name, "hilite") == 0) {
			colors.hilite = parsed_color;
			colors.hilite_bold = parsed_bold;
		} else if (strcasecmp(name, "bracket") == 0) {
			colors.bracket = parsed_color;
			colors.bracket_bold = parsed_bold;
		}
		/* Just for symmetry */
		else {
			continue;
		}
	}

	free(color_env);
	return colors;
}

/*
 * Set the output color mode
 *
 * t: the terminal to set the color for
 * color: the color to being using
 */
EINFO_NONNULL
static void _ecolor(struct einfo_term *t, ECOLOR color)
{
	const char *env_temp = NULL;
	struct color_map colors;
	int target_color;
	bool target_bold;
	const char *color_str [] = { NULL, NULL };

	if (!prepare_term(t)) {
		return;
	}

	env_temp = getenv("EINFO_COLOR");
	if (!yesno(env_temp)) {
		/* Only skip colors on a true "no" */
		if (errno == 0) {
			return;
		}
	}
	colors = env_colors(env_temp);

	if (color != ECOLOR_NORMAL) {
		if (color == ECOLOR_GOOD) {
			target_bold = colors.good_bold;
			target_color = colors.good;
		} else if (color == ECOLOR_WARN) {
			target_bold = colors.warn_bold;
			target_color = colors.warn;
		} else if (color == ECOLOR_BAD) {
			target_bold = colors.bad_bold;
			target_color = colors.bad;
		} else if (color == ECOLOR_HILITE) {
			target_bold = colors.hilite_bold;
			target_color = colors.hilite;
		} else if (color == ECOLOR_BRACKET) {
			target_bold = colors.bracket_bold;
			target_color = colors.bracket;
		}
		color_str[0] = target_bold
			? enter_bold_mode
			: exit_attribute_mode;
		color_str[1] = tiparm(set_a_foreground, target_color);
	}
	/* ECOLOR_NORMAL */
	else {
		color_str[0] = tiparm(orig_pair);
		color_str[1] = tiparm(exit_attribute_mode);
	}

	tputs(color_str[0], 1, _putc);
	tputs(color_str[1], 1, _putc);
}

/*
 * Move up one row
 *
 * t: terminal to move in
 */
EINFO_NONNULL
static void _move_up(struct einfo_term *t)
{
	const char *move_str = "";

	if (!prepare_term(t)) {
		return;
	}

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
 */
EINFO_NONNULL
static void _move_col(struct einfo_term *t, int col)
{
	const char *move_str = "";

	if (!prepare_term(t)) {
		return;
	}

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
 */
EINFO_NONNULL
static int _eindent(FILE *f)
{
	/* Pre-terminated :) */
	char indent [INDENT_MAX + 1] = { 0 };
	const char *env_level_str = getenv("EINFO_INDENT");
	size_t env_level = 0;

	/*
	 * Use the indent level from the env if it exists and is valid. Given as
	 * a count of spaces.
	 */
	if (env_level_str != NULL) {
		env_level = chomp_strtoul(env_level_str, INDENT_MAX);

		if (env_level == 0 && errno == 0) {
			return 0;
		} else if (errno != EINVAL) {
			memset(indent, ' ', env_level);
			return fprintf(f, "%s", indent);
		}
	}

	/*
	 * The indent_level bounds are checked in eindent()/eoutdent(), this is
	 * just to skip unneeded work.
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
 */
EINFO_NONNULL
EINFO_PRINTF(3, 0)
static int _einfo(
	struct einfo_term *t,
	ECOLOR color,
	const char *EINFO_RESTRICT fmt,
	va_list va)
{
	const char *last_env = getenv(LASTCMD_ENV);

	int ret = 0;
	va_list ap;

	va_copy(ap, va);

	/* Let's assume the env var is "more correct" */
	if (last_env != NULL) {
		last = last_env;
	}

	/*
	 * Print a newline on non-curses terms if the last command is a
	 * "no newline" variant. Why only non-curses? IDFK.
	 */
	if (!prepare_term(t)
		&& last != NULL
		&& last[strlen(last) - 1] == 'n'
		&& strcmp(last, "ewarn") != 0) {
		fprintf(t->file, "\n");
	}

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

	if (last_env != NULL) {
		/* The env var has served its purpose */
		unsetenv(LASTCMD_ENV);
	}
	last = NULL;

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
		msg = "...";
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
 */
EINFO_NONNULL
EINFO_PRINTF(2, 0)
static void _elog(int level, const char *EINFO_RESTRICT fmt, va_list va)
{
	const char *id = getenv("EINFO_LOG");

	va_list ap;

	if (id) {
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

	if (!fmt) {
		return;
	}

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
/* TODO: why is this not sent to syslog? */
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
/* TODO: why is this not sent to syslog? */
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
int ewarn(const char * EINFO_RESTRICT fmt, ...)
{
	int ret;
	va_list ap;

	if (!fmt || is_quiet()) {
		return 0;
	}

	va_start(ap, fmt);

	_elog(LOG_WARNING, fmt, ap);
	ret = _einfo(&stderr_term, ECOLOR_WARN, fmt, ap);
	ret += fprintf(stderr_term.file, "\n");

	va_end(ap);
	LASTCMD(__func__);
	return ret;
}
void ewarnx(const char * EINFO_RESTRICT fmt, ...)
{
	va_list ap;

	if (fmt && !is_quiet()) {
		va_start(ap, fmt);

		_elog(LOG_WARNING, fmt, ap);
		_einfo(&stderr_term, ECOLOR_WARN, fmt, ap);
		fprintf(stderr_term.file, "\n");

		va_end(ap);
	}

	exit(EXIT_FAILURE);
}
int eerror(const char * EINFO_RESTRICT fmt, ...)
{
	int ret;
	va_list ap;

	if (!fmt || is_really_quiet()) {
		return 0;
	}

	va_start(ap, fmt);

	_elog(LOG_ERR, fmt, ap);
	ret = _einfo(&stderr_term, ECOLOR_BAD, fmt, ap);
	ret += fprintf(stderr_term.file, "\n");

	va_end(ap);
	LASTCMD(__func__);
	return ret;
}
void eerrorx(const char * EINFO_RESTRICT fmt, ...)
{
	va_list ap;

	if (fmt && !is_really_quiet()) {
		va_start(ap, fmt);

		_elog(LOG_ERR, fmt, ap);
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
/* TODO: why is this not sent to syslog? */
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
/* TODO: why is this not sent to syslog? */
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
