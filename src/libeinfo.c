/*
   einfo.c
   Gentoo informational functions
   Copyright 2007 Gentoo Foundation
   */

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

#include "einfo.h"
#include "rc.h"
#include "rc-misc.h"

/* Incase we cannot work out how many columns from ioctl, supply a default */
#define DEFAULT_COLS		 80

#define OK			"ok"
#define NOT_OK			"!!"

#define CHECK_VERBOSE		if (! is_env ("RC_VERBOSE", "yes")) return 0

/* Number of spaces for an indent */
#define INDENT_WIDTH		2

/* How wide can the indent go? */
#define INDENT_MAX		40

#define EBUFFER_LOCK		RC_SVCDIR "ebuffer/.lock"

/* A cheat sheet of colour capable terminals
   This is taken from DIR_COLORS from GNU coreutils
   We embed it here as we shouldn't depend on coreutils */
static const char *colour_terms[] =
{
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

bool colour_terminal (void)
{
  static int in_colour = -1;
  int i = 0;
  char *term;

  if (in_colour == 0)
    return (false);
  if (in_colour == 1)
    return (true);

  term = getenv ("TERM");
  /* If $TERM isn't set then the chances are we're in single user mode */
  if (! term)
    return (true);

  while (colour_terms[i])
    {
      if (strcmp (colour_terms[i], term) == 0)
        {
          in_colour = 1;
          return (true);
        }
      i++;
    }

  in_colour = 0;
  return (false);
}

static int get_term_columns (void)
{
#ifdef TIOCGSIZE /* BSD */
  struct ttysize ts;

  if (ioctl(0, TIOCGSIZE, &ts) == 0)
    return (ts.ts_cols);
#elif TIOCGWINSZ /* Linux */
  struct winsize ws; 

  if (ioctl(0, TIOCGWINSZ, &ws) == 0)
    return (ws.ws_col);
#endif

  return (DEFAULT_COLS);
}

static int ebuffer (const char *cmd, int retval, const char *fmt, va_list ap)
{
  char *file = getenv ("RC_EBUFFER");
  FILE *fp;
  char buffer[RC_LINEBUFFER];
  int l = 1;

  if (! file || ! cmd || strlen (cmd) < 4)
    return (0);

  if (! (fp = fopen (file, "a")))
    {
      fprintf (stderr, "fopen `%s': %s\n", file, strerror (errno));
      return (0);
    }

  fprintf (fp, "%s %d ", cmd, retval);

  if (fmt)
    {
      va_list apc;
      va_copy (apc, ap);
      l = vsnprintf (buffer, sizeof (buffer), fmt, apc);
      fprintf (fp, "%d %s\n", l, buffer);
      va_end (apc);
    }
  else
    fprintf (fp, "0\n");

  fclose (fp);
  return (l);
}

typedef struct func
{
  const char *name;
  int (*efunc) (const char *fmt, ...);
  int (*eefunc) (int retval, const char *fmt, ...);
  void (*eind) (void);
} func_t;

static const func_t funcmap[] = {
    { "einfon", &einfon, NULL, NULL },
    { "ewarnn", &ewarnn, NULL, NULL},
    { "eerrorn", &eerrorn, NULL, NULL},
    { "einfo", &einfo, NULL, NULL },
    { "ewarn", &ewarn, NULL, NULL },
    { "eerror", &eerror, NULL, NULL },
    { "ebegin", &ebegin, NULL, NULL },
    { "eend", NULL, &eend, NULL },
    { "ewend", NULL, &ewend, NULL },
    { "eindent", NULL, NULL, &eindent },
    { "eoutdent", NULL, NULL, &eoutdent },
    { "einfovn", &einfovn, NULL, NULL },
    { "ewarnvn", &ewarnvn, NULL, NULL },
    { "einfov", &einfov, NULL, NULL },
    { "ewarnv", &ewarnv, NULL, NULL },
    { "ebeginv", &ebeginv, NULL, NULL },
    { "eendv", NULL, &eendv, NULL },
    { "ewendv", NULL, &ewendv, NULL },
    { "eindentv" ,NULL, NULL, &eindentv },
    { "eoutdentv", NULL, NULL, &eoutdentv },
    { NULL, NULL, NULL, NULL },
};

void eflush (void)
{
  FILE *fp;
  char *file = getenv ("RC_EBUFFER");
  char buffer[RC_LINEBUFFER];
  char *cmd;
  int retval = 0;
  int length = 0;
  char *token;
  char *p;
  struct stat buf;
  pid_t pid;
  char newfile[PATH_MAX];
  int i = 1;

  if (! file|| (stat (file, &buf) != 0))
    {
      errno = 0;
      return;
    }

  /* Find a unique name for our file */
  while (true)
    {
      snprintf (newfile, sizeof (newfile), "%s.%d", file, i);
      if (stat (newfile, &buf) != 0)
        {
          if (rename (file, newfile))
            fprintf (stderr, "rename `%s' `%s': %s\n", file, newfile,
                     strerror (errno));
          break;
        }
      i++;
    }

  /* We fork a child process here so we don't hold anything up */
  if ((pid = fork ()) == -1)
    {
      fprintf (stderr, "fork: %s", strerror (errno));
      return;
    }

  if (pid != 0)
    return;

  /* Spin until we can lock the ebuffer */
  while (true)
    {
      struct timeval tv;
      tv.tv_sec = 0;
      tv.tv_usec = 20000;
      select (0, NULL, NULL, NULL, &tv);
      errno = 0;
      if (link (newfile, EBUFFER_LOCK) == 0)
        break;
      if (errno != EEXIST)
        fprintf (stderr, "link `%s' `%s': %s\n", newfile, EBUFFER_LOCK,
                 strerror (errno));
    }

  if (! (fp = fopen (newfile, "r")))
    {
      fprintf (stderr, "fopen `%s': %s\n", newfile, strerror (errno));
      return;
    }

  unsetenv ("RC_EBUFFER");

  memset (buffer, 0, RC_LINEBUFFER);
  while (fgets (buffer, RC_LINEBUFFER, fp))
    {
      i = strlen (buffer) - 1;
      if (i < 1)
        continue;

      if (buffer[i] == '\n')
        buffer[i] = 0;

      p = buffer;
      cmd = strsep (&p, " ");
      token = strsep (&p, " ");
      if (sscanf (token, "%d", &retval) != 1)
        {
          fprintf (stderr, "eflush `%s': not a number", token);
          continue;
        }
      token = strsep (&p, " ");
      if (sscanf (token, "%d", &length) != 1)
        {
          fprintf (stderr, "eflush `%s': not a number", token);
          continue;
        }

      i = 0;
      while (funcmap[i].name)
        {
          if (strcmp (funcmap[i].name, cmd) == 0)
            {
              if (funcmap[i].efunc)
                {
                  if (p)
                    funcmap[i].efunc ("%s", p);
                  else
                    funcmap[i].efunc (NULL, NULL);
                }
              else if (funcmap[i].eefunc)
                {
                  if (p)
                    funcmap[i].eefunc (retval, "%s", p);
                  else
                    funcmap[i].eefunc (retval, NULL, NULL);
                }
              else if (funcmap[i].eind)
                funcmap[i].eind ();
              else
                fprintf (stderr, "eflush `%s': no function defined\n", cmd);
              break;
            }
          i++;
        }

      if (! funcmap[i].name)
        fprintf (stderr, "eflush `%s': invalid function\n", cmd);
    }
  fclose (fp);

  if (unlink (EBUFFER_LOCK))
    fprintf (stderr, "unlink `%s': %s", EBUFFER_LOCK, strerror (errno));

  if (unlink (newfile))
    fprintf (stderr, "unlink `%s': %s", newfile, strerror (errno));

  _exit (EXIT_SUCCESS);
}

#define EBUFFER(_cmd, _retval, _fmt, _ap) \
{ \
  int _i = ebuffer (_cmd, _retval, _fmt, _ap); \
  if (_i) \
  return (_i); \
}

static void elog (int level, const char *fmt, va_list ap)
{
  char *e = getenv ("RC_ELOG");
  va_list apc;

  if (fmt && e)
    {
      closelog ();
      openlog (e, LOG_PID, LOG_DAEMON);
      va_copy (apc, ap);
      vsyslog (level, fmt, apc);
      va_end (apc);
      closelog ();
    }
}
static int _eindent (FILE *stream)
{
  char *env = getenv ("RC_EINDENT");
  int amount = 0;
  char indent[INDENT_MAX];

  if (env)
    {
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

#define EINFOVN(_file, _colour) \
 if (colour_terminal ()) \
fprintf (_file, " " _colour "*" EINFO_NORMAL " "); \
else \
fprintf (_file, " * "); \
retval += _eindent (_file); \
{ \
  va_list _ap; \
  va_copy (_ap, ap); \
  retval += vfprintf (_file, fmt, _ap) + 3; \
  va_end (_ap); \
} \
if (colour_terminal ()) \
fprintf (_file, "\033[K");

static int _einfovn (const char *fmt, va_list ap)
{
  int retval = 0;

  EINFOVN (stdout, EINFO_GOOD);
  return (retval);
}

static int _ewarnvn (const char *fmt, va_list ap)
{
  int retval = 0;

  EINFOVN (stdout, EINFO_WARN);
  return (retval);
}

static int _eerrorvn (const char *fmt, va_list ap)
{
  int retval = 0;

  EINFOVN (stderr, EINFO_BAD);
  return (retval);
}

int einfon (const char *fmt, ...)
{
  int retval;
  va_list ap;

  if (! fmt || is_env ("RC_QUIET", "yes"))
    return (0);

  va_start (ap, fmt);
  if (! (retval = ebuffer ("einfon", 0, fmt, ap)))
    retval = _einfovn (fmt, ap);
  va_end (ap);

  return (retval);
}

int ewarnn (const char *fmt, ...)
{
  int retval;
  va_list ap;

  if (! fmt || is_env ("RC_QUIET", "yes"))
    return (0);

  va_start (ap, fmt);
  if (! (retval = ebuffer ("ewarnn", 0, fmt, ap)))
    retval = _ewarnvn (fmt, ap);
  va_end (ap);

  return (retval);
}

int eerrorn (const char *fmt, ...)
{
  int retval;
  va_list ap;

  va_start (ap, fmt);
  if (! (retval = ebuffer ("eerrorn", 0, fmt, ap)))
    retval = _eerrorvn (fmt, ap);
  va_end (ap);

  return (retval);
}

int einfo (const char *fmt, ...)
{
  int retval;
  va_list ap;

  if (! fmt || is_env ("RC_QUIET", "yes"))
    return (0);

  va_start (ap, fmt);
  if (! (retval = ebuffer ("einfo", 0, fmt, ap)))
    {
      retval = _einfovn (fmt, ap);
      retval += printf ("\n");
    }
  va_end (ap);

  return (retval);
}

int ewarn (const char *fmt, ...)
{
  int retval;
  va_list ap;

  if (! fmt || is_env ("RC_QUIET", "yes"))
    return (0);

  va_start (ap, fmt);
  elog (LOG_WARNING, fmt, ap);
  if (! (retval = ebuffer ("ewarn", 0, fmt, ap)))
    {
      retval = _ewarnvn (fmt, ap);
      retval += printf ("\n");
    }
  va_end (ap);

  return (retval);
}

void ewarnx (const char *fmt, ...)
{
  int retval;
  va_list ap;

  if (fmt && ! is_env ("RC_QUIET", "yes"))
    {
      va_start (ap, fmt);
      elog (LOG_WARNING, fmt, ap);
      retval = _ewarnvn (fmt, ap);
      va_end (ap);
      retval += printf ("\n");
    }
  exit (EXIT_FAILURE);
}

int eerror (const char *fmt, ...)
{
  int retval;
  va_list ap;

  if (! fmt)
    return (0);

  va_start (ap, fmt);
  elog (LOG_ERR, fmt, ap);
  retval = _eerrorvn (fmt, ap);
  va_end (ap);
  retval += fprintf (stderr, "\n");

  return (retval);
}

void eerrorx (const char *fmt, ...)
{
  va_list ap;

  if (fmt)
    {
      va_start (ap, fmt);
      elog (LOG_ERR, fmt, ap);
      _eerrorvn (fmt, ap);
      va_end (ap);
      printf ("\n");
    }
  exit (EXIT_FAILURE);
}

int ebegin (const char *fmt, ...)
{
  int retval;
  va_list ap;

  if (! fmt || is_env ("RC_QUIET", "yes"))
    return (0);

  va_start (ap, fmt);
  if ((retval = ebuffer ("ebegin", 0, fmt, ap)))
    {
      va_end (ap);
      return (retval);
    }

  retval = _einfovn (fmt, ap);
  va_end (ap);
  retval += printf (" ...");
  if (colour_terminal ())
    retval += printf ("\n");

  return (retval);
}

static void _eend (int col, einfo_color_t color, const char *msg)
{
  FILE *fp = stdout;
  int i;
  int cols;

  if (! msg)
    return;

  if (color == einfo_bad)
    fp = stderr;

  cols = get_term_columns () - (strlen (msg) + 6);

  if (cols > 0 && colour_terminal ())
    {
      fprintf (fp, "\033[A\033[%dC %s[ ", cols, EINFO_BRACKET);
      switch (color)
        {
        case einfo_good:
          fprintf (fp, EINFO_GOOD);
          break;
        case einfo_warn:
          fprintf (fp, EINFO_WARN);
          break;
        case einfo_bad:
          fprintf (fp, EINFO_BAD);
          break;
        case einfo_hilite:
          fprintf (fp, EINFO_HILITE);
          break;
        case einfo_bracket:
          fprintf (fp, EINFO_BRACKET);
          break;
        case einfo_normal:
          fprintf (fp, EINFO_NORMAL);
          break;
        }
      fprintf (fp, "%s%s ]%s\n", msg, EINFO_BRACKET, EINFO_NORMAL);
    }
  else
    {
      for (i = -1; i < cols - col; i++)
        fprintf (fp, " ");
      fprintf (fp, "[ %s ]\n", msg);
    }
}

static int _do_eend (const char *cmd, int retval, const char *fmt, va_list ap)
{
  int col = 0;
  FILE *fp;
  va_list apc;
  int eb;

  if (fmt)
    {
      va_copy (apc, ap);
      eb = ebuffer (cmd, retval, fmt, apc);
      va_end (apc);
      if (eb)
        return (retval);
    }

  if (fmt && retval != 0)
    {
      va_copy (apc, ap);
      if (strcmp (cmd, "ewend") == 0)
        {
          col = _ewarnvn (fmt, apc);
          fp = stdout;
        }
      else
        {
          col = _eerrorvn (fmt, apc);
          fp = stderr;
        }
      va_end (apc);
      if (colour_terminal ())
        fprintf (fp, "\n");
    }

  _eend (col, retval == 0 ? einfo_good : einfo_bad, retval == 0 ? OK : NOT_OK);
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

void ebracket (int col, einfo_color_t color, const char *msg)
{
  _eend (col, color, msg);
}

void eindent (void)
{
  char *env = getenv ("RC_EINDENT");
  int amount = 0;
  char num[10];

  if (ebuffer ("eindent", 0, NULL, NULL))
    return;

  if (env)
    {
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

void eoutdent (void)
{
  char *env = getenv ("RC_EINDENT");
  int amount = 0;
  char num[10];

  if (ebuffer ("eoutdent", 0, NULL, NULL))
    return;

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
  else
    {
      snprintf (num, 10, "%08d", amount);
      setenv ("RC_EINDENT", num, 1);
    }
}

int einfovn (const char *fmt, ...)
{
  int retval;
  va_list ap;

  CHECK_VERBOSE;

  if (! fmt)
    return (0);

  va_start (ap, fmt);
  if (! (retval = ebuffer ("einfovn", 0, fmt, ap)))
    retval = _einfovn (fmt, ap);
  va_end (ap);

  return (retval);
}

int ewarnvn (const char *fmt, ...)
{
  int retval;
  va_list ap;

  CHECK_VERBOSE;

  if (! fmt)
    return (0);

  va_start (ap, fmt);
  if (! (retval = ebuffer ("ewarnvn", 0, fmt, ap)))
    retval = _ewarnvn (fmt, ap);
  va_end (ap);

  return (retval);
}

int einfov (const char *fmt, ...)
{
  int retval;
  va_list ap;

  CHECK_VERBOSE;

  if (! fmt)
    return (0);

  va_start (ap, fmt);
  if (! (retval = ebuffer ("einfov", 0, fmt, ap)))
    {
      retval = _einfovn (fmt, ap);
      retval += printf ("\n");
    }
  va_end (ap);

  return (retval);
}

int ewarnv (const char *fmt, ...)
{
  int retval;
  va_list ap;

  CHECK_VERBOSE;

  if (! fmt)
    return (0);

  va_start (ap, fmt);
  if (! (retval = ebuffer ("ewarnv", 0, fmt, ap)))
    {
      retval = _ewarnvn (fmt, ap);
      retval += printf ("\n");
    }
  va_end (ap);
  retval += printf ("\n");

  return (retval);
}

int ebeginv (const char *fmt, ...)
{
  int retval;
  va_list ap;

  CHECK_VERBOSE;

  if (! fmt)
    return (0);

  va_start (ap, fmt);
  if (! (retval = ebuffer ("ebeginv", 0, fmt, ap)))
    {
      retval = _einfovn (fmt, ap);
      retval += printf (" ...");
      if (colour_terminal ())
        retval += printf ("\n");
    }
  va_end (ap);

  return (retval);
}

int eendv (int retval, const char *fmt, ...)
{
  va_list ap;

  CHECK_VERBOSE;

  va_start (ap, fmt);
  _do_eend ("eendv", retval, fmt, ap);
  va_end (ap);

  return (retval);
}

int ewendv (int retval, const char *fmt, ...)
{
  va_list ap;

  CHECK_VERBOSE;

  va_start (ap, fmt);
  _do_eend ("ewendv", retval, fmt, ap);
  va_end (ap);

  return (retval);
}

void eindentv (void)
{
  if (is_env ("RC_VERBOSE", "yes"))
    eindent ();
}

void eoutdentv (void)
{
  if (is_env ("RC_VERBOSE", "yes"))
    eoutdent ();
}
