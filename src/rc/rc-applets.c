/*
   rc-applets.c

   Handle multicall applets for use in our init scripts.
   Basically this makes us a lot faster for the most part, and removes
   any shell incompatabilities we might otherwise encounter.
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

#define SYSLOG_NAMES

#include <sys/types.h>
#include <errno.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

#include "builtins.h"
#include "einfo.h"
#include "rc-misc.h"
#include "strlist.h"

/* Applet is first parsed in rc.c - no point in doing it again */
extern const char *applet;

static int syslog_decode (char *name, CODE *codetab)
{
	CODE *c;

	if (isdigit ((int) *name))
		return (atoi (name));

	for (c = codetab; c->c_name; c++)
		if (! strcasecmp (name, c->c_name))
			return (c->c_val);

	return (-1);
}

static int do_e (int argc, char **argv)
{
	int retval = EXIT_SUCCESS;
	int i;
	int l = 0;
	char *message = NULL;
	char *p;
	int level = 0;

	/* Punt applet */
	argc--;
	argv++;

	if (strcmp (applet, "eval_ecolors") == 0) {
		printf ("GOOD='%s'\nWARN='%s'\nBAD='%s'\nHILITE='%s'\nBRACKET='%s'\nNORMAL='%s'\n",
			ecolor (ECOLOR_GOOD),
			ecolor (ECOLOR_WARN),
			ecolor (ECOLOR_BAD),
			ecolor (ECOLOR_HILITE),
			ecolor (ECOLOR_BRACKET),
			ecolor (ECOLOR_NORMAL));
		exit (EXIT_SUCCESS);
	}

	if (argc > 0) {

		if (strcmp (applet, "eend") == 0 ||
		    strcmp (applet, "ewend") == 0 ||
		    strcmp (applet, "veend") == 0 ||
		    strcmp (applet, "vweend") == 0)
		{
			errno = 0;
			retval = strtol (argv[0], NULL, 0);
			if (errno != 0)
				retval = EXIT_FAILURE;
			else {
				argc--;
				argv++;
			}
		} else if (strcmp (applet, "esyslog") == 0 ||
			   strcmp (applet, "elog") == 0) {
			char *dot = strchr (argv[0], '.');
			if ((level = syslog_decode (dot + 1, prioritynames)) == -1)
				eerrorx ("%s: invalid log level `%s'", applet, argv[0]);

			if (argc < 3)
				eerrorx ("%s: not enough arguments", applet);

			unsetenv ("EINFO_LOG");
			setenv ("EINFO_LOG", argv[1], 1);

			argc -= 2;
			argv += 2;
		}
	}

	if (argc > 0) {
		for (i = 0; i < argc; i++)
			l += strlen (argv[i]) + 1;

		message = xmalloc (l);
		p = message;

		for (i = 0; i < argc; i++) {
			if (i > 0)
				*p++ = ' ';
			memcpy (p, argv[i], strlen (argv[i]));
			p += strlen (argv[i]);
		}
		*p = 0;
	}

	if (! message)
		message = xstrdup ("");

	if (strcmp (applet, "einfo") == 0)
		einfo ("%s", message);
	else if (strcmp (applet, "einfon") == 0)
		einfon ("%s", message);
	else if (strcmp (applet, "ewarn") == 0)
		ewarn ("%s", message);
	else if (strcmp (applet, "ewarnn") == 0)
		ewarnn ("%s", message);
	else if (strcmp (applet, "eerror") == 0) {
		eerror ("%s", message);
		retval = 1;
	} else if (strcmp (applet, "eerrorn") == 0) {
		eerrorn ("%s", message);
		retval = 1;
	} else if (strcmp (applet, "ebegin") == 0)
		ebegin ("%s", message);
	else if (strcmp (applet, "eend") == 0)
		eend (retval, "%s", message);
	else if (strcmp (applet, "ewend") == 0)
		ewend (retval, "%s", message);
	else if (strcmp (applet, "esyslog") == 0)
		elog (level, "%s", message);
	else if (strcmp (applet, "veinfo") == 0)
		einfov ("%s", message);
	else if (strcmp (applet, "veinfon") == 0)
		einfovn ("%s", message);
	else if (strcmp (applet, "vewarn") == 0)
		ewarnv ("%s", message);
	else if (strcmp (applet, "vewarnn") == 0)
		ewarnvn ("%s", message);
	else if (strcmp (applet, "vebegin") == 0)
		ebeginv ("%s", message);
	else if (strcmp (applet, "veend") == 0)
		eendv (retval, "%s", message);
	else if (strcmp (applet, "vewend") == 0)
		ewendv (retval, "%s", message);
	else if (strcmp (applet, "eindent") == 0)
		eindent ();
	else if (strcmp (applet, "eoutdent") == 0)
		eoutdent ();
	else if (strcmp (applet, "veindent") == 0)
		eindentv ();
	else if (strcmp (applet, "veoutdent") == 0)
		eoutdentv ();
	else {
		eerror ("%s: unknown applet", applet);
		retval = EXIT_FAILURE;
	}

	free (message);
	return (retval);
}

static int do_service (int argc, char **argv)
{
	bool ok = false;
	char *service = NULL;

	if (argc > 1)
		service = argv[1];
	else
		service = getenv ("SVCNAME");

	if (! service || strlen (service) == 0)
		eerrorx ("%s: no service specified", applet);

	if (strcmp (applet, "service_started") == 0)
		ok = (rc_service_state (service) & RC_SERVICE_STARTED);
	else if (strcmp (applet, "service_stopped") == 0)
		ok = (rc_service_state (service) & RC_SERVICE_STOPPED);
	else if (strcmp (applet, "service_inactive") == 0)
		ok = (rc_service_state (service) & RC_SERVICE_INACTIVE);
	else if (strcmp (applet, "service_starting") == 0)
		ok = (rc_service_state (service) & RC_SERVICE_STARTING);
	else if (strcmp (applet, "service_stopping") == 0)
		ok = (rc_service_state (service) & RC_SERVICE_STOPPING);
	else if (strcmp (applet, "service_coldplugged") == 0)
		ok = (rc_service_state (service) & RC_SERVICE_COLDPLUGGED);
	else if (strcmp (applet, "service_wasinactive") == 0)
		ok = (rc_service_state (service) & RC_SERVICE_WASINACTIVE);
	else if (strcmp (applet, "service_started_daemon") == 0) {
		int idx = 0;
		char *d = argv[1];

		service = getenv ("SVCNAME");
		if (argc > 3) {
			service = argv[0];
			d = argv[2];
			sscanf (argv[3], "%d", &idx);
		} else if (argc == 3) {
			sscanf (argv[2], "%d", &idx);
		}
		exit (rc_service_started_daemon (service, d, idx) ? 0 : 1);
	} else
		eerrorx ("%s: unknown applet", applet);

	return (ok ? EXIT_SUCCESS : EXIT_FAILURE);
}

static int do_mark_service (int argc, char **argv)
{
	bool ok = false;
	char *svcname = getenv ("SVCNAME");
	char *service = NULL;

	if (argc > 1)
		service = argv[1];
	else
		service = getenv ("SVCNAME");

	if (! service || strlen (service) == 0)
		eerrorx ("%s: no service specified", applet);

	if (strcmp (applet, "mark_service_started") == 0)
		ok = rc_service_mark (service, RC_SERVICE_STARTED);
	else if (strcmp (applet, "mark_service_stopped") == 0)
		ok = rc_service_mark (service, RC_SERVICE_STOPPED);
	else if (strcmp (applet, "mark_service_inactive") == 0)
		ok = rc_service_mark (service, RC_SERVICE_INACTIVE);
	else if (strcmp (applet, "mark_service_starting") == 0)
		ok = rc_service_mark (service, RC_SERVICE_STARTING);
	else if (strcmp (applet, "mark_service_stopping") == 0)
		ok = rc_service_mark (service, RC_SERVICE_STOPPING);
	else if (strcmp (applet, "mark_service_coldplugged") == 0)
		ok = rc_service_mark (service, RC_SERVICE_COLDPLUGGED);
	else if (strcmp (applet, "mark_service_failed") == 0)
		ok = rc_service_mark (service, RC_SERVICE_FAILED);
	else
		eerrorx ("%s: unknown applet", applet);

	/* If we're marking ourselves then we need to inform our parent runscript
	   process so they do not mark us based on our exit code */
	if (ok && svcname && strcmp (svcname, service) == 0) {
		char *runscript_pid = getenv ("RC_RUNSCRIPT_PID");
		char *mtime;
		pid_t pid = 0;
		int l;

		if (runscript_pid && sscanf (runscript_pid, "%d", &pid) == 1)
			if (kill (pid, SIGHUP) != 0)
				eerror ("%s: failed to signal parent %d: %s",
					applet, pid, strerror (errno));

		/* Remove the exclusive time test. This ensures that it's not
		   in control as well */
		l = strlen (RC_SVCDIR "exclusive") +
			strlen (svcname) +
			strlen (runscript_pid) +
			4;
		mtime = xmalloc (l);
		snprintf (mtime, l, RC_SVCDIR "exclusive/%s.%s",
			  svcname, runscript_pid);
		if (exists (mtime) && unlink (mtime) != 0)
			eerror ("%s: unlink: %s", applet, strerror (errno));
		free (mtime);
	}

	return (ok ? EXIT_SUCCESS : EXIT_FAILURE);
}

static int do_value (int argc, char **argv)
{
	bool ok = false;
	char *service = getenv ("SVCNAME");

	if (! service)
		eerrorx ("%s: no service specified", applet);

	if (argc < 2 || ! argv[1] || strlen (argv[1]) == 0)
		eerrorx ("%s: no option specified", applet);

	if (strcmp (applet, "service_get_value") == 0 ||
	    strcmp (applet, "get_options") == 0)
	{
		char *option = rc_service_value_get (service, argv[1]);
		if (option) {
			printf ("%s", option);
			free (option);
			ok = true;
		}
	} else if (strcmp (applet, "service_set_value") == 0 ||
		   strcmp (applet, "save_options") == 0)
		ok = rc_service_value_set (service, argv[1], argv[2]);
	else
		eerrorx ("%s: unknown applet", applet);

	return (ok ? EXIT_SUCCESS : EXIT_FAILURE);
}

static int do_shell_var (int argc, char **argv)
{
	int i;

	for (i = 1; i < argc; i++) {
		char *p = argv[i];

		if (i != 1)
			putchar (' ');

		while (*p) {
			char c = *p++;
			if (! isalnum ((int) c))
				c = '_';
			putchar (c);
		}
	}
	putchar ('\n');

	return (EXIT_SUCCESS);
}


void run_applets (int argc, char **argv)
{
	/* These are designed to be applications in their own right */
	if (strcmp (applet, "fstabinfo") == 0)
		exit (fstabinfo (argc, argv));
	else if (strcmp (applet, "mountinfo") == 0)
		exit (mountinfo (argc, argv));
	else if (strcmp (applet, "rc-depend") == 0)
		exit (rc_depend (argc, argv));
	else if (strcmp (applet, "rc-status") == 0)
		exit (rc_status (argc, argv));
	else if (strcmp (applet, "rc-update") == 0 ||
		 strcmp (applet, "update-rc") == 0)
		exit (rc_update (argc, argv));
	else if (strcmp (applet, "runscript") == 0)
		exit (runscript (argc, argv));
	else if (strcmp (applet, "start-stop-daemon") == 0)
		exit (start_stop_daemon (argc, argv));
	else if (strcmp (applet, "checkpath") == 0)
		exit (checkpath (argc, argv));

	/* These could also be applications in their own right */
	if (strcmp (applet, "shell_var") == 0)
		exit (do_shell_var (argc, argv));

	if (strcmp (applet, "is_newer_than") == 0 ||
	    strcmp (applet, "is_older_than") == 0)
	{
		bool match = false;
		int i = 2;

		if (argc < 3)
			exit (EXIT_FAILURE);
		if (strcmp (applet, "is_newer_than") == 0)
			match = true;
		while (i < argc) {
			if (rc_newer_than (argv[1], argv[i++]) != match)
				exit (EXIT_FAILURE);
		}
		exit (EXIT_SUCCESS);
	}; 

	if (applet[0] == 'e' || (applet[0] == 'v' && applet[1] == 'e'))
		exit (do_e (argc, argv));

	/* These are purely for init scripts and do not make sense as
	 * anything else */
	if (strcmp (applet, "service_get_value") == 0 ||
	    strcmp (applet, "service_set_value") == 0 ||
	    strcmp (applet, "get_options") == 0 ||
	    strcmp (applet, "save_options") == 0)
		exit (do_value (argc, argv));

	if (strncmp (applet, "service_", strlen ("service_")) == 0)
		exit (do_service (argc, argv));

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
}
