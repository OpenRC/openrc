/*
   rc.c
   rc - manager for init scripts which control the startup, shutdown
   and the running of daemons on a Gentoo system.

   Also a multicall binary for various commands that can be used in shell
   scripts to query service state, mark service state and provide the
   Gentoo einfo family of informational functions.

   Copyright 2007 Gentoo Foundation
   Released under the GPLv2
   */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <sys/wait.h>
#include <errno.h>
#include <ctype.h>
#include <libgen.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include "einfo.h"
#include "rc.h"
#include "rc-misc.h"
#include "rc-plugin.h"
#include "strlist.h"

#define INITSH                  RC_LIBDIR "sh/init.sh"
#define INITEARLYSH             RC_LIBDIR "sh/init-early.sh"
#define HALTSH                  RC_INITDIR "halt.sh"
#define SULOGIN                 "/sbin/sulogin"

#define RC_SVCDIR_STARTING      RC_SVCDIR "starting/"
#define RC_SVCDIR_INACTIVE      RC_SVCDIR "inactive/"
#define RC_SVCDIR_STARTED       RC_SVCDIR "started/"
#define RC_SVCDIR_COLDPLUGGED   RC_SVCDIR "coldplugged/"

#define INTERACTIVE             RC_SVCDIR "interactive"

#define DEVBOOT					"/dev/.rcboot"

/* Cleanup anything in main */
#define CHAR_FREE(_item) if (_item) { \
	free (_item); \
	_item = NULL; \
}

extern char **environ;

static char *applet = NULL;
static char **env = NULL;
static char **newenv = NULL;
static char **coldplugged_services = NULL;
static char **stop_services = NULL;
static char **start_services = NULL;
static rc_depinfo_t *deptree = NULL;
static char **types = NULL;
static char *tmp = NULL;

struct termios *termios_orig = NULL;

typedef struct pidlist
{
	pid_t pid;
	struct pidlist *next;
} pidlist_t;
static pidlist_t *service_pids = NULL;

static void cleanup (void)
{
	pidlist_t *pl = service_pids;

	rc_plugin_unload ();

	if (termios_orig) {
		tcsetattr (STDIN_FILENO, TCSANOW, termios_orig);
		free (termios_orig);
	}

	while (pl) {
		pidlist_t *p = pl->next;
		free (pl);
		pl = p;
	}

	rc_strlist_free (env);
	rc_strlist_free (newenv);
	rc_strlist_free (coldplugged_services);
	rc_strlist_free (stop_services);
	rc_strlist_free (start_services);
	rc_free_deptree (deptree);
	rc_strlist_free (types);

	/* Clean runlevel start, stop markers */
	if (rc_is_dir (RC_SVCDIR "softscripts.new"))
		rc_rm_dir (RC_SVCDIR "softscripts.new", true);
	if (rc_is_dir (RC_SVCDIR "softscripts.old"))
		rc_rm_dir (RC_SVCDIR "softscripts.old", true);

	free (applet);
}

static int do_e (int argc, char **argv)
{
	int retval = EXIT_SUCCESS;
	int i;
	int l = 0;
	char *message = NULL;
	char *p;
	char *fmt = NULL;

	if (strcmp (applet, "eval_ecolors") == 0) {
		printf ("GOOD='%s'\nWARN='%s'\nBAD='%s'\nHILITE='%s'\nBRACKET='%s'\nNORMAL='%s'\n",
				ecolor (ecolor_good),
				ecolor (ecolor_warn),
				ecolor (ecolor_bad),
				ecolor (ecolor_hilite),
				ecolor (ecolor_bracket),
				ecolor (ecolor_normal));
		exit (EXIT_SUCCESS);
	}

	if (strcmp (applet, "eend") == 0 ||
		strcmp (applet, "ewend") == 0 ||
		strcmp (applet, "veend") == 0 ||
		strcmp (applet, "vweend") == 0)
	{
		if (argc > 0) {
			errno = 0;
			retval = strtol (argv[0], NULL, 0);
			if (errno != 0)
				retval = EXIT_FAILURE;
			else {
				argc--;
				argv++;
			}
		}
		else
			retval = EXIT_FAILURE;
	}

	if (argc > 0) {
		for (i = 0; i < argc; i++)
			l += strlen (argv[i]) + 1;

		message = rc_xmalloc (l);
		p = message;

		for (i = 0; i < argc; i++) {	
			if (i > 0)
				*p++ = ' ';
			memcpy (p, argv[i], strlen (argv[i]));
			p += strlen (argv[i]);
		}
		*p = 0;
	}

	if (message)
		fmt = rc_xstrdup ("%s");

	if (strcmp (applet, "einfo") == 0) 
		einfo (fmt, message);
	else if (strcmp (applet, "einfon") == 0)
		einfon (fmt, message);
	else if (strcmp (applet, "ewarn") == 0) 
		ewarn (fmt, message);
	else if (strcmp (applet, "ewarnn") == 0)
		ewarnn (fmt, message);
	else if (strcmp (applet, "eerror") == 0) {
		eerror (fmt, message);
		retval = 1;
	} else if (strcmp (applet, "eerrorn") == 0) {
		eerrorn (fmt, message);
		retval = 1;
	} else if (strcmp (applet, "ebegin") == 0)
		ebegin (fmt, message);
	else if (strcmp (applet, "eend") == 0)
		eend (retval, fmt, message);
	else if (strcmp (applet, "ewend") == 0)
		ewend (retval, fmt, message);
	else if (strcmp (applet, "veinfo") == 0) 
		einfov (fmt, message);
	else if (strcmp (applet, "veinfon") == 0)
		einfovn (fmt, message);
	else if (strcmp (applet, "vewarn") == 0) 
		ewarnv (fmt, message);
	else if (strcmp (applet, "vewarnn") == 0)
		ewarnvn (fmt, message);
	else if (strcmp (applet, "vebegin") == 0)
		ebeginv (fmt, message);
	else if (strcmp (applet, "veend") == 0)
		eendv (retval, fmt, message);
	else if (strcmp (applet, "vewend") == 0)
		ewendv (retval, fmt, message);
	else if (strcmp (applet, "eindent") == 0)
		eindent ();
	else if (strcmp (applet, "eoutdent") == 0)
		eoutdent ();
	else if (strcmp (applet, "veindent") == 0)
		eindentv ();
	else if (strcmp (applet, "veoutdent") == 0)
		eoutdentv ();
	else if (strcmp (applet, "eflush") == 0)
		eflush ();
	else {
		eerror ("%s: unknown applet", applet);
		retval = EXIT_FAILURE;
	}

	if (fmt)
		free (fmt);
	if (message)
		free (message);
	return (retval);
}

static int do_service (int argc, char **argv)
{
	bool ok = false;

	if (argc < 1 || ! argv[0] || strlen (argv[0]) == 0)
		eerrorx ("%s: no service specified", applet);

	if (strcmp (applet, "service_started") == 0)
		ok = rc_service_state (argv[0], rc_service_started);
	else if (strcmp (applet, "service_stopped") == 0)
		ok = rc_service_state (argv[0], rc_service_stopped);
	else if (strcmp (applet, "service_inactive") == 0)
		ok = rc_service_state (argv[0], rc_service_inactive);
	else if (strcmp (applet, "service_starting") == 0)
		ok = rc_service_state (argv[0], rc_service_starting);
	else if (strcmp (applet, "service_stopping") == 0)
		ok = rc_service_state (argv[0], rc_service_stopping);
	else if (strcmp (applet, "service_coldplugged") == 0)
		ok = rc_service_state (argv[0], rc_service_coldplugged);
	else if (strcmp (applet, "service_wasinactive") == 0)
		ok = rc_service_state (argv[0], rc_service_wasinactive);
	else if (strcmp (applet, "service_started_daemon") == 0) {
		int idx = 0;
		if (argc > 2)
			sscanf (argv[2], "%d", &idx);
		exit (rc_service_started_daemon (argv[0], argv[1], idx)
			  ? 0 : 1);
	} else
		eerrorx ("%s: unknown applet", applet);

	return (ok ? EXIT_SUCCESS : EXIT_FAILURE);
}

static int do_mark_service (int argc, char **argv)
{
	bool ok = false;
	char *svcname = getenv ("SVCNAME");

	if (argc < 1 || ! argv[0] || strlen (argv[0]) == 0)
		eerrorx ("%s: no service specified", applet);

	if (strcmp (applet, "mark_service_started") == 0)
		ok = rc_mark_service (argv[0], rc_service_started);
	else if (strcmp (applet, "mark_service_stopped") == 0)
		ok = rc_mark_service (argv[0], rc_service_stopped);
	else if (strcmp (applet, "mark_service_inactive") == 0)
		ok = rc_mark_service (argv[0], rc_service_inactive);
	else if (strcmp (applet, "mark_service_starting") == 0)
		ok = rc_mark_service (argv[0], rc_service_starting);
	else if (strcmp (applet, "mark_service_stopping") == 0)
		ok = rc_mark_service (argv[0], rc_service_stopping);
	else if (strcmp (applet, "mark_service_coldplugged") == 0)
		ok = rc_mark_service (argv[0], rc_service_coldplugged);
	else
		eerrorx ("%s: unknown applet", applet);

	/* If we're marking ourselves then we need to inform our parent runscript
	   process so they do not mark us based on our exit code */
	if (ok && svcname && strcmp (svcname, argv[0]) == 0) {
		char *runscript_pid = getenv ("RC_RUNSCRIPT_PID");
		char *mtime;
		pid_t pid = 0;
		int l;

		if (runscript_pid && sscanf (runscript_pid, "%d", &pid) == 1)
			if (kill (pid, SIGHUP) != 0)
				eerror ("%s: failed to signal parent %d: %s",
						applet, pid, strerror (errno));

		/* Remove the exclsive time test. This ensures that it's not
		   in control as well */
		l = strlen (RC_SVCDIR "exclusive") +
			strlen (svcname) +
			strlen (runscript_pid) +
			4;
		mtime = rc_xmalloc (l);
		snprintf (mtime, l, RC_SVCDIR "exclusive/%s.%s",
				  svcname, runscript_pid);
		if (rc_exists (mtime) && unlink (mtime) != 0)
			eerror ("%s: unlink: %s", applet, strerror (errno));
		free (mtime);
	}

	return (ok ? EXIT_SUCCESS : EXIT_FAILURE);
}

static int do_options (int argc, char **argv)
{
	bool ok = false;
	char *service = getenv ("SVCNAME");

	if (! service)
		eerrorx ("%s: no service specified", applet);

	if (argc < 1 || ! argv[0] || strlen (argv[0]) == 0)
		eerrorx ("%s: no option specified", applet);

	if (strcmp (applet, "get_options") == 0) {
		char buffer[1024];
		memset (buffer, 0, 1024);
		ok = rc_get_service_option (service, argv[0], buffer);
		if (ok)
			printf ("%s", buffer);
	} else if (strcmp (applet, "save_options") == 0)
		ok = rc_set_service_option (service, argv[0], argv[1]);
	else
		eerrorx ("%s: unknown applet", applet);

	return (ok ? EXIT_SUCCESS : EXIT_FAILURE);
}

static char read_key (bool block)
{
	struct termios termios;
	char c = 0;

	if (! isatty (STDIN_FILENO))
		return (false);

	/* Now save our terminal settings. We need to restore them at exit as we
	   will be changing it for non-blocking reads for Interactive */
	if (! termios_orig) {
		termios_orig = rc_xmalloc (sizeof (struct termios));
		tcgetattr (STDIN_FILENO, termios_orig);
	}

	tcgetattr (STDIN_FILENO, &termios);
	termios.c_lflag &= ~(ICANON | ECHO);
	if (block)
		termios.c_cc[VMIN] = 1;
	else {
		termios.c_cc[VMIN] = 0;
		termios.c_cc[VTIME] = 0;
	}
	tcsetattr (STDIN_FILENO, TCSANOW, &termios);

	read (STDIN_FILENO, &c, 1);

	tcsetattr (STDIN_FILENO, TCSANOW, termios_orig);

	return (c);
}

static bool want_interactive (void)
{
	char c = read_key (false);
	return ((c == 'I' || c == 'i') ? true : false);
}

static void mark_interactive (void)
{
	FILE *fp = fopen (INTERACTIVE, "w");
	if (fp)
		fclose (fp);
}

static void sulogin (bool cont)
{
#ifdef __linux__
	char *e = getenv ("RC_SYS");

	/* VPS systems cannot do an sulogin */
	if (e && strcmp (e, "VPS") == 0) {
		execl ("/sbin/halt", "/sbin/halt", "-f", (char *) NULL);
		eerrorx ("%s: unable to exec `/sbin/halt': %s", applet, strerror (errno));
	}
#endif

	newenv = rc_filter_env ();

	if (cont) {
		int status = 0;
#ifdef __linux__
		char *tty = ttyname (fileno (stdout));
#endif

		pid_t pid = vfork ();

		if (pid == -1)
			eerrorx ("%s: vfork: %s", applet, strerror (errno));
		if (pid == 0) {
#ifdef __linux__
			if (tty)
				execle (SULOGIN, SULOGIN, tty, (char *) NULL, newenv);
			else
				execle (SULOGIN, SULOGIN, (char *) NULL, newenv);

			eerror ("%s: unable to exec `%s': %s", applet, SULOGIN,
					strerror (errno));
#else
			execle ("/bin/sh", "/bin/sh", (char *) NULL, newenv);
			eerror ("%s: unable to exec `/bin/sh': %s", applet,
					strerror (errno));
#endif
			_exit (EXIT_FAILURE);
		}
		waitpid (pid, &status, 0);
	} else {
#ifdef __linux
		execle ("/sbin/sulogin", "/sbin/sulogin", (char *) NULL, newenv);
		eerrorx ("%s: unable to exec `/sbin/sulogin': %s", applet, strerror (errno));
#else
		exit (EXIT_SUCCESS);
#endif
	}
}

static void single_user (void)
{
#ifdef __linux__
	execl ("/sbin/telinit", "/sbin/telinit", "S", (char *) NULL);
	eerrorx ("%s: unable to exec `/sbin/telinit': %s",
			 applet, strerror (errno));
#else
	if (kill (1, SIGTERM) != 0)
		eerrorx ("%s: unable to send SIGTERM to init (pid 1): %s",
				 applet, strerror (errno));
	exit (EXIT_SUCCESS);
#endif
}

static void set_ksoftlevel (const char *runlevel)
{
	FILE *fp;

	if (! runlevel ||
		strcmp (runlevel, RC_LEVEL_BOOT) == 0 ||
		strcmp (runlevel, RC_LEVEL_SINGLE) == 0 ||
		strcmp (runlevel, RC_LEVEL_SYSINIT) == 0)
	{
		if (rc_exists (RC_SVCDIR "ksoftlevel") &&
			unlink (RC_SVCDIR "ksoftlevel") != 0)
			eerror ("unlink `%s': %s", RC_SVCDIR "ksoftlevel", strerror (errno));
		return;
	}

	if (! (fp = fopen (RC_SVCDIR "ksoftlevel", "w"))) {
		eerror ("fopen `%s': %s", RC_SVCDIR "ksoftlevel", strerror (errno));
		return;
	}

	fprintf (fp, "%s", runlevel);
	fclose (fp);
}

static void wait_for_services ()
{
	int status = 0;
	while (wait (&status) != -1);
}

static void add_pid (pid_t pid)
{
	pidlist_t *sp = service_pids;
	if (sp) {
		while (sp->next)
			sp = sp->next;
		sp->next = rc_xmalloc (sizeof (pidlist_t));
		sp = sp->next;
	} else
		sp = service_pids = rc_xmalloc (sizeof (pidlist_t));
	memset (sp, 0, sizeof (pidlist_t));
	sp->pid = pid;
}

static void remove_pid (pid_t pid)
{
	pidlist_t *last = NULL;
	pidlist_t *pl;

	for (pl = service_pids; pl; pl = pl->next) {
		if (pl->pid == pid) {
			if (last)
				last->next = pl->next;
			else
				service_pids = pl->next;
			free (pl);
			break;
		}
		last = pl;
	}
}

static void handle_signal (int sig)
{
	int serrno = errno;
	char signame[10] = { '\0' };
	char *run;
	char *prev;
	pidlist_t *pl;
	pid_t pid;
	int status = 0;

	switch (sig) {
		case SIGCHLD:
			do {
				pid = waitpid (-1, &status, WNOHANG);
				if (pid < 0) {
					if (errno != ECHILD)
						eerror ("waitpid: %s", strerror (errno));
					return;
				}
			} while (! WIFEXITED (status) && ! WIFSIGNALED (status));

			/* Remove that pid from our list */
			if (pid > 0)
				remove_pid (pid);
			break;

		case SIGINT:
			if (! signame[0])
				snprintf (signame, sizeof (signame), "SIGINT");
		case SIGTERM:
			if (! signame[0])
				snprintf (signame, sizeof (signame), "SIGTERM");
		case SIGQUIT:
			if (! signame[0])
				snprintf (signame, sizeof (signame), "SIGQUIT");
			eerrorx ("%s: caught %s, aborting", applet, signame);
		case SIGUSR1:
			eerror ("rc: Aborting!");
			/* Kill any running services we have started */

			signal (SIGCHLD, SIG_IGN);
			for (pl = service_pids; pl; pl = pl->next)
				kill (pl->pid, SIGTERM);

			/* Notify plugins we are aborting */
			rc_plugin_run (rc_hook_abort, NULL);

			/* Only drop into single user mode if we're booting */
			run = getenv ("RUNLEVEL");
			prev = getenv ("PREVLEVEL");
			if ((prev &&
				 (strcmp (prev, "S") == 0 ||
				  strcmp (prev, "1") == 0)) ||
				 (run &&
				  (strcmp (run, "S") == 0 ||
				   strcmp (run, "1") == 0)))
				single_user ();

			exit (EXIT_FAILURE);
			break;

		default:
			eerror ("%s: caught unknown signal %d", applet, sig);
	}

	/* Restore errno */
	errno = serrno;
}

int main (int argc, char **argv)
{
	char *RUNLEVEL = NULL;
	char *PREVLEVEL = NULL;
	char *runlevel = NULL;
	char *newlevel = NULL;
	char *service = NULL;
	char **deporder = NULL;
	int i = 0;
	int j = 0;
	bool going_down = false;
	bool interactive = false;
	int depoptions = RC_DEP_STRICT | RC_DEP_TRACE;
	char ksoftbuffer [PATH_MAX];
	char pidstr[6];

	if (argv[0])
		applet = rc_xstrdup (basename (argv[0]));

	if (! applet)
		eerrorx ("arguments required");

	argc--;
	argv++;

	/* Handle multicall stuff */
	if (applet[0] == 'e' || (applet[0] == 'v' && applet[1] == 'e'))
		exit (do_e (argc, argv));

	if (strncmp (applet, "service_", strlen ("service_")) == 0)
		exit (do_service (argc, argv));

	if (strcmp (applet, "get_options") == 0 ||
		strcmp (applet, "save_options") == 0)
		exit (do_options (argc, argv));

	if (strncmp (applet, "mark_service_", strlen ("mark_service_")) == 0)
		exit (do_mark_service (argc, argv));

	if (strcmp (applet, "is_runlevel_start") == 0)
		exit (rc_runlevel_starting () ? 0 : 1);
	else if (strcmp (applet, "is_runlevel_stop") == 0)
		exit (rc_runlevel_stopping () ? 0 : 1);

	if (strcmp (applet, "rc-abort") == 0) {
		char *p = getenv ("RC_PID");
		pid_t pid = 0;

		if (p && sscanf (p, "%d", &pid) == 1) {
			if (kill (pid, SIGUSR1) != 0)
				eerrorx ("rc-abort: failed to signal parent %d: %s",
						 pid, strerror (errno));
			exit (EXIT_SUCCESS);
		}
		exit (EXIT_FAILURE);
	}

	if (strcmp (applet, "rc" ) != 0)
		eerrorx ("%s: unknown applet", applet);

	/* OK, so we really are the main RC process
	   Only root should be able to run us */
	if (geteuid () != 0)
		eerrorx ("%s: root access required", applet);

	atexit (cleanup);
	newlevel = argv[0];

	/* Setup a signal handler */
	signal (SIGINT, handle_signal);
	signal (SIGQUIT, handle_signal);
	signal (SIGTERM, handle_signal);
	signal (SIGUSR1, handle_signal);

	/* Ensure our environment is pure
	   Also, add our configuration to it */
	env = rc_filter_env ();
	env = rc_config_env (env);

	if (env) {
		char *p;

#ifdef __linux__
		/* clearenv isn't portable, but there's no harm in using it
		   if we have it */
		clearenv ();
#else
		char *var;
		/* No clearenv present here then.
		   We could manipulate environ directly ourselves, but it seems that
		   some kernels bitch about this according to the environ man pages
		   so we walk though environ and call unsetenv for each value. */
		while (environ[0]) {
			tmp = rc_xstrdup (environ[0]);
			p = tmp;
			var = strsep (&p, "=");
			unsetenv (var);
			free (tmp);
		}
		tmp = NULL;
#endif

		STRLIST_FOREACH (env, p, i)
			if (strcmp (p, "RC_SOFTLEVEL") != 0 && strcmp (p, "SOFTLEVEL") != 0)
				putenv (p);

		/* We don't free our list as that would be null in environ */
	}

	/* Enable logging */
	setenv ("RC_ELOG", "rc", 1);

	/* Export our PID */
	snprintf (pidstr, sizeof (pidstr), "%d", getpid ());
	setenv ("RC_PID", pidstr, 1);

	interactive = rc_exists (INTERACTIVE);
	rc_plugin_load ();

	/* RUNLEVEL is set by sysvinit as is a magic number
	   RC_SOFTLEVEL is set by us and is the name for this magic number
	   even though all our userland documentation refers to runlevel */
	RUNLEVEL = getenv ("RUNLEVEL");
	PREVLEVEL = getenv ("PREVLEVEL");

	if (RUNLEVEL && newlevel) {
		if (strcmp (RUNLEVEL, "S") == 0 || strcmp (RUNLEVEL, "1") == 0) {
			/* OK, we're either in runlevel 1 or single user mode */
			if (strcmp (newlevel, RC_LEVEL_SYSINIT) == 0) {
				struct utsname uts;
				pid_t pid;
				pid_t wpid;
				int status = 0;
#ifdef __linux__
				FILE *fp;
#endif

                /* exec init-early.sh if it exists
                 * This should just setup the console to use the correct
                 * font. Maybe it should setup the keyboard too? */
                if (rc_exists (INITEARLYSH)) {
                    if ((pid = vfork ()) == -1)
                        eerrorx ("%s: vfork: %s", applet, strerror (errno));

                    if (pid == 0) {
                        execl (INITEARLYSH, INITEARLYSH, (char *) NULL);
                        eerror ("%s: unable to exec `" INITEARLYSH "': %s",
                                applet, strerror (errno));
                        _exit (EXIT_FAILURE);
                    }

                    do {
                        wpid = waitpid (pid, &status, 0);
                        if (wpid < 1)
                            eerror ("waitpid: %s", strerror (errno));
                    } while (! WIFEXITED (status) && ! WIFSIGNALED (status));
                }

				uname (&uts);

				printf ("\n");
				printf ("   %sGentoo/%s; %shttp://www.gentoo.org/%s"
						"\n   Copyright 1999-2007 Gentoo Foundation; "
						"Distributed under the GPLv2\n\n",
						ecolor (ecolor_good), uts.sysname, ecolor (ecolor_bracket),
						ecolor (ecolor_normal));

				printf ("Press %sI%s to enter interactive boot mode\n\n",
						ecolor (ecolor_good), ecolor (ecolor_normal));

				setenv ("RC_SOFTLEVEL", newlevel, 1);
				rc_plugin_run (rc_hook_runlevel_start_in, newlevel);

				if ((pid = vfork ()) == -1)
					eerrorx ("%s: vfork: %s", applet, strerror (errno));

				if (pid == 0) {
					execl (INITSH, INITSH, (char *) NULL);
					eerror ("%s: unable to exec `" INITSH "': %s",
							applet, strerror (errno));
					_exit (EXIT_FAILURE);
				}

				do {
					wpid = waitpid (pid, &status, 0);
					if (wpid < 1)
						eerror ("waitpid: %s", strerror (errno));
				} while (! WIFEXITED (status) && ! WIFSIGNALED (status));

				if (! WIFEXITED (status) || ! WEXITSTATUS (status) == 0)
					exit (EXIT_FAILURE);

				/* If we requested a softlevel, save it now */
#ifdef __linux__
				set_ksoftlevel (NULL);

				if ((fp = fopen ("/proc/cmdline", "r"))) {
					char buffer[RC_LINEBUFFER];
					char *soft;

					memset (buffer, 0, sizeof (buffer));
					if (fgets (buffer, RC_LINEBUFFER, fp) &&
						(soft = strstr (buffer, "softlevel=")))
					{ 
						i = soft - buffer;
						if (i  == 0 || buffer[i - 1] == ' ') {
							char *level;

							/* Trim the trailing carriage return if present */
							i = strlen (buffer) - 1;
							if (buffer[i] == '\n')
								buffer[i] = 0;

							soft += strlen ("softlevel=");
							level = strsep (&soft, " ");
							set_ksoftlevel (level);
						}
					}
					fclose (fp);
				}
#endif
				rc_plugin_run (rc_hook_runlevel_start_out, newlevel);

				if (want_interactive ())
					mark_interactive ();

				exit (EXIT_SUCCESS);
			}

#ifdef __linux__
			/* Parse the inittab file so we can work out the level to telinit */
			if (strcmp (newlevel, RC_LEVEL_BOOT) != 0 &&
				strcmp (newlevel, RC_LEVEL_SINGLE) != 0)
			{
				char **inittab = rc_get_list (NULL, "/etc/inittab");
				char *line;
				char *p;
				char *token;
				char lvl[2] = {0, 0};

				STRLIST_FOREACH (inittab, line, i) {
					p = line;
					token = strsep (&p, ":");
					if (! token || token[0] != 'l')
						continue;

					if ((token = strsep (&p, ":")) == NULL)
						continue;

					/* Snag the level */
					lvl[0] = token[0];

					/* The name is spaced after this */
					if ((token = strsep (&p, " ")) == NULL)
						continue;

					if ((token = strsep (&p, " ")) == NULL)
						continue;

					if (strcmp (token, newlevel) == 0)
						break;
				}
				rc_strlist_free (inittab);

				/* We have a level, so telinit into it */
				if (lvl[0] == 0) {
					eerrorx ("%s: couldn't find a runlevel called `%s'",
							 applet, newlevel);
				} else {
					execl ("/sbin/telinit", "/sbin/telinit", lvl, (char *) NULL);
					eerrorx ("%s: unable to exec `/sbin/telinit': %s",
							 applet, strerror (errno));
				}
			} 
#endif
		}
	}

	/* Check we're in the runlevel requested, ie from
	   rc single
	   rc shutdown
	   rc reboot
	   */
	if (newlevel) {
		if (strcmp (newlevel, RC_LEVEL_SINGLE) == 0) {
			if (! RUNLEVEL ||
				(strcmp (RUNLEVEL, "S") != 0 &&
				 strcmp (RUNLEVEL, "1") != 0))
			{
				/* Remember the current runlevel for when we come back */
				set_ksoftlevel (runlevel);
				single_user ();
			}
		} else if (strcmp (newlevel, RC_LEVEL_REBOOT) == 0) {
			if (! RUNLEVEL ||
				strcmp (RUNLEVEL, "6") != 0)
			{
				execl ("/sbin/shutdown", "/sbin/shutdown", "-r", "now", (char *) NULL);
				eerrorx ("%s: unable to exec `/sbin/shutdown': %s",
						 applet, strerror (errno));
			}
		} else if (strcmp (newlevel, RC_LEVEL_SHUTDOWN) == 0) {
			if (! RUNLEVEL ||
				strcmp (RUNLEVEL, "0") != 0)
			{
				execl ("/sbin/shutdown", "/sbin/shutdown",
#ifdef __linux
					   "-h",
#else
					   "-p",
#endif
					   "now", (char *) NULL);
				eerrorx ("%s: unable to exec `/sbin/shutdown': %s",
						 applet, strerror (errno));
			}
		}
	}

	/* Export our current softlevel */
	runlevel = rc_get_runlevel ();

	/* Now we start handling our children */
	signal (SIGCHLD, handle_signal);

	/* If we're in the default runlevel and ksoftlevel exists, we should use
	   that instead */
	if (newlevel &&
		rc_exists (RC_SVCDIR "ksoftlevel") &&
		strcmp (newlevel, RC_LEVEL_DEFAULT) == 0)
	{
		/* We should only use ksoftlevel if we were in single user mode
		   If not, we need to erase ksoftlevel now. */
		if (PREVLEVEL &&
			(strcmp (PREVLEVEL, "1") == 0 ||
			 strcmp (PREVLEVEL, "S") == 0 ||
			 strcmp (PREVLEVEL, "N") == 0))
		{
			FILE *fp;

			if (! (fp = fopen (RC_SVCDIR "ksoftlevel", "r")))
				eerror ("fopen `%s': %s", RC_SVCDIR "ksoftlevel",
						strerror (errno));
			else {
				if (fgets (ksoftbuffer, sizeof (ksoftbuffer), fp)) {
					i = strlen (ksoftbuffer) - 1;
					if (ksoftbuffer[i] == '\n')
						ksoftbuffer[i] = 0;
					newlevel = ksoftbuffer;
				}
				fclose (fp);
			}
		} else
			set_ksoftlevel (NULL);
	}

	if (newlevel &&
		(strcmp (newlevel, RC_LEVEL_REBOOT) == 0 ||
		 strcmp (newlevel, RC_LEVEL_SHUTDOWN) == 0 ||
		 strcmp (newlevel, RC_LEVEL_SINGLE) == 0))
	{
		going_down = true;
		rc_set_runlevel (newlevel);
		setenv ("RC_SOFTLEVEL", newlevel, 1);
		rc_plugin_run (rc_hook_runlevel_stop_in, newlevel);
	} else {
		rc_plugin_run (rc_hook_runlevel_stop_in, runlevel);
	}

	/* Check if runlevel is valid if we're changing */
	if (newlevel && strcmp (runlevel, newlevel) != 0 && ! going_down) {
		tmp = rc_strcatpaths (RC_RUNLEVELDIR, newlevel, (char *) NULL);
		if (! rc_is_dir (tmp))
			eerrorx ("%s: is not a valid runlevel", newlevel);
		CHAR_FREE (tmp);
	}

	/* Load our deptree now */
	if ((deptree = rc_load_deptree ()) == NULL)
		eerrorx ("failed to load deptree");

	/* Clean the failed services state dir now */
	if (rc_is_dir (RC_SVCDIR "failed"))
		rc_rm_dir (RC_SVCDIR "failed", false);

	mkdir (RC_SVCDIR "/softscripts.new", 0755);

#ifdef __linux__
	/* udev likes to start services before we're ready when it does
	   its coldplugging thing. runscript knows when we're not ready so it
	   stores a list of coldplugged services in DEVBOOT for us to pick up
	   here when we are ready for them */
	if (rc_is_dir (DEVBOOT)) {
		start_services = rc_ls_dir (NULL, DEVBOOT, RC_LS_INITD);
		rc_rm_dir (DEVBOOT, true);

		STRLIST_FOREACH (start_services, service, i)
			if (rc_allow_plug (service))
				rc_mark_service (service, rc_service_coldplugged);
		/* We need to dump this list now.
		   This may seem redunant, but only Linux needs this and saves on
		   code bloat. */
		rc_strlist_free (start_services);
		start_services = NULL;
	}
#else
	/* BSD's on the other hand populate /dev automagically and use devd.
	   The only downside of this approach and ours is that we have to hard code
	   the device node to the init script to simulate the coldplug into
	   runlevel for our dependency tree to work. */
	if (newlevel && strcmp (newlevel, RC_LEVEL_BOOT) == 0 &&
		(strcmp (runlevel, RC_LEVEL_SINGLE) == 0 ||
		 strcmp (runlevel, RC_LEVEL_SYSINIT) == 0) &&
		rc_is_env ("RC_COLDPLUG", "yes"))
	{
		/* The net interfaces are easy - they're all in net /dev/net :) */
		start_services = rc_ls_dir (NULL, "/dev/net", 0);
		STRLIST_FOREACH (start_services, service, i) {
			j = (strlen ("net.") + strlen (service) + 1);
			tmp = rc_xmalloc (sizeof (char *) * j);
			snprintf (tmp, j, "net.%s", service);
			if (rc_service_exists (tmp) && rc_allow_plug (tmp))
				rc_mark_service (tmp, rc_service_coldplugged);
			CHAR_FREE (tmp);
		}
		rc_strlist_free (start_services);

		/* The mice are a little more tricky.
		   If we coldplug anything else, we'll probably do it here. */
		start_services = rc_ls_dir (NULL, "/dev", 0);
		STRLIST_FOREACH (start_services, service, i) {
			if (strncmp (service, "psm", 3) == 0 ||
				strncmp (service, "ums", 3) == 0)
			{
				char *p = service + 3;
				if (p && isdigit (*p)) {
					j = (strlen ("moused.") + strlen (service) + 1);
					tmp = rc_xmalloc (sizeof (char *) * j);
					snprintf (tmp, j, "moused.%s", service);
					if (rc_service_exists (tmp) && rc_allow_plug (tmp))
						rc_mark_service (tmp, rc_service_coldplugged);
					CHAR_FREE (tmp);
				}
			}
		}
		rc_strlist_free (start_services);
		start_services = NULL;
	}
#endif

	/* Build a list of all services to stop and then work out the
	   correct order for stopping them */
	stop_services = rc_ls_dir (stop_services, RC_SVCDIR_STARTING, RC_LS_INITD);
	stop_services = rc_ls_dir (stop_services, RC_SVCDIR_INACTIVE, RC_LS_INITD);
	stop_services = rc_ls_dir (stop_services, RC_SVCDIR_STARTED, RC_LS_INITD);

	types = rc_strlist_add (NULL, "ineed");
	types = rc_strlist_add (types, "iuse");
	types = rc_strlist_add (types, "iafter");
	deporder = rc_get_depends (deptree, types, stop_services,
							   runlevel, depoptions);
	rc_strlist_free (stop_services);
	rc_strlist_free (types);
	stop_services = deporder;
	deporder = NULL;
	types = NULL;
	rc_strlist_reverse (stop_services);

	/* Load our list of coldplugged services */
	coldplugged_services = rc_ls_dir (coldplugged_services,
									  RC_SVCDIR_COLDPLUGGED, RC_LS_INITD);

	/* Load our start services now.
	   We have different rules dependent on runlevel. */
	if (newlevel && strcmp (newlevel, RC_LEVEL_BOOT) == 0) {
		if (coldplugged_services) {
			einfon ("Device initiated services:");
			STRLIST_FOREACH (coldplugged_services, service, i) {
				printf (" %s", service);
				start_services = rc_strlist_add (start_services, service);
			}
			printf ("\n");
		}
		tmp = rc_strcatpaths (RC_RUNLEVELDIR, newlevel ? newlevel : runlevel,
							  (char *) NULL);
		start_services = rc_ls_dir (start_services, tmp, RC_LS_INITD);
		CHAR_FREE (tmp);
	} else {
		/* Store our list of coldplugged services */
		coldplugged_services = rc_ls_dir (coldplugged_services, RC_SVCDIR_COLDPLUGGED,
										  RC_LS_INITD);
		if (strcmp (newlevel ? newlevel : runlevel, RC_LEVEL_SINGLE) != 0 &&
			strcmp (newlevel ? newlevel : runlevel, RC_LEVEL_SHUTDOWN) != 0 &&
			strcmp (newlevel ? newlevel : runlevel, RC_LEVEL_REBOOT) != 0)
		{
			/* We need to include the boot runlevel services if we're not in it */
			start_services = rc_ls_dir (start_services, RC_RUNLEVELDIR RC_LEVEL_BOOT,
										RC_LS_INITD);
			STRLIST_FOREACH (coldplugged_services, service, i)
				start_services = rc_strlist_add (start_services, service);

			tmp = rc_strcatpaths (RC_RUNLEVELDIR,
								  newlevel ? newlevel : runlevel, (char *) NULL);
			start_services = rc_ls_dir (start_services, tmp, RC_LS_INITD);
			CHAR_FREE (tmp);
		}
	}

	/* Save out softlevel now */
	if (going_down)
		rc_set_runlevel (newlevel);

	types = rc_strlist_add (NULL, "needsme");
	types = rc_strlist_add (types, "usesme");
	/* Now stop the services that shouldn't be running */
	STRLIST_FOREACH (stop_services, service, i) {
		bool found = false;
		char *conf = NULL;
		char **stopdeps = NULL;
		char *svc1 = NULL;
		char *svc2 = NULL;
		int k;

		if (rc_service_state (service, rc_service_stopped))
			continue;

		/* We always stop the service when in these runlevels */
		if (going_down) {
			pid_t pid = rc_stop_service (service);
			if (pid > 0 && ! rc_is_env ("RC_PARALLEL", "yes"))
				rc_waitpid (pid);
			continue;
		}

		/* If we're in the start list then don't bother stopping us */
		STRLIST_FOREACH (start_services, svc1, j)
			if (strcmp (svc1, service) == 0) {
				found = true;
				break;
			}

		/* Unless we would use a different config file */
		if (found) {
			int len;
			if (! newlevel)
				continue;

			len = strlen (service) + strlen (runlevel) + 2;
			tmp = rc_xmalloc (sizeof (char *) * len);
			snprintf (tmp, len, "%s.%s", service, runlevel);
			conf = rc_strcatpaths (RC_CONFDIR, tmp, (char *) NULL);
			found = rc_exists (conf);
			CHAR_FREE (conf);
			CHAR_FREE (tmp);
			if (! found) {
				len = strlen (service) + strlen (newlevel) + 2;
				tmp = rc_xmalloc (sizeof (char *) * len);
				snprintf (tmp, len, "%s.%s", service, newlevel);
				conf = rc_strcatpaths (RC_CONFDIR, tmp, (char *) NULL);
				found = rc_exists (conf);
				CHAR_FREE (conf);
				CHAR_FREE (tmp);
				if (!found)
					continue;
			}
		} else {
			/* Allow coldplugged services not to be in the runlevels list */
			if (rc_service_state (service, rc_service_coldplugged))
				continue;
		}

		/* We got this far! Or last check is to see if any any service that
		   going to be started depends on us */
		stopdeps = rc_strlist_add (stopdeps, service);
		deporder = rc_get_depends (deptree, types, stopdeps,
								   runlevel, RC_DEP_STRICT);
		rc_strlist_free (stopdeps);
		stopdeps = NULL;
		found = false;
		STRLIST_FOREACH (deporder, svc1, j) {
			STRLIST_FOREACH (start_services, svc2, k)
				if (strcmp (svc1, svc2) == 0) {
					found = true;
					break;
				}
			if (found)
				break;
		}
		rc_strlist_free (deporder);
		deporder = NULL;

		/* After all that we can finally stop the blighter! */
		if (! found) {
			pid_t pid = rc_stop_service (service);
			if (pid > 0 && ! rc_is_env ("RC_PARALLEL", "yes"))
				rc_waitpid (pid);
		}
	}
	rc_strlist_free (types);
	types = NULL;

	/* Wait for our services to finish */
	wait_for_services ();

	/* Notify the plugins we have finished */
	rc_plugin_run (rc_hook_runlevel_stop_out, runlevel);

	rmdir (RC_SVCDIR "/softscripts.new");

	/* Store the new runlevel */
	if (newlevel) {
		rc_set_runlevel (newlevel);
		runlevel = newlevel;
		setenv ("RC_SOFTLEVEL", runlevel, 1);
	}

	/* Run the halt script if needed */
	if (strcmp (runlevel, RC_LEVEL_SHUTDOWN) == 0 ||
		strcmp (runlevel, RC_LEVEL_REBOOT) == 0)
	{
		execl (HALTSH, HALTSH, runlevel, (char *) NULL);
		eerrorx ("%s: unable to exec `%s': %s",
				 applet, HALTSH, strerror (errno));
	}

	/* Single user is done now */
	if (strcmp (runlevel, RC_LEVEL_SINGLE) == 0) {
		if (rc_exists (INTERACTIVE))
			unlink (INTERACTIVE);
		sulogin (false);
	}

	mkdir (RC_SVCDIR "softscripts.old", 0755);
	rc_plugin_run (rc_hook_runlevel_start_in, runlevel);

	/* Re-add our coldplugged services if they stopped */
	STRLIST_FOREACH (coldplugged_services, service, i)
		rc_mark_service (service, rc_service_coldplugged);

	/* Order the services to start */
	types = rc_strlist_add (NULL, "ineed");
	types = rc_strlist_add (types, "iuse");
	types = rc_strlist_add (types, "iafter");
	deporder = rc_get_depends (deptree, types, start_services,
							   runlevel, depoptions);
	rc_strlist_free (types);
	types = NULL;
	rc_strlist_free (start_services);
	start_services = deporder;
	deporder = NULL;

	STRLIST_FOREACH (start_services, service, i) {
		if (rc_service_state (service, rc_service_stopped))	{
			pid_t pid;

			if (! interactive)
				interactive = want_interactive ();

			if (interactive) {
interactive_retry:
				printf ("\n");
				einfo ("About to start the service %s", service);
				eindent ();
				einfo ("1) Start the service\t\t2) Skip the service");
				einfo ("3) Continue boot process\t\t4) Exit to shell");
				eoutdent ();
interactive_option:
				switch (read_key (true)) {
					case '1': break;
					case '2': continue;
					case '3': interactive = false; break;
					case '4': sulogin (true); goto interactive_retry;
					default: goto interactive_option;
				}
			}

			/* Remember the pid if we're running in parallel */
			if ((pid = rc_start_service (service)))
				add_pid (pid);

			if (! rc_is_env ("RC_PARALLEL", "yes")) {
				rc_waitpid (pid);
				remove_pid (pid);
			}
		}
	}

	/* Wait for our services to finish */
	wait_for_services ();

	rc_plugin_run (rc_hook_runlevel_start_out, runlevel);

	/* Store our interactive status for boot */
	if (interactive && strcmp (runlevel, RC_LEVEL_BOOT) == 0)
		mark_interactive ();
	else {
		if (rc_exists (INTERACTIVE))
			unlink (INTERACTIVE);
	}

	return (EXIT_SUCCESS);
}

