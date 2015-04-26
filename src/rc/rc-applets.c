/*
  rc-applets.c

  Handle multicall applets for use in our init scripts.
  Basically this makes us a lot faster for the most part, and removes
  any shell incompatabilities we might otherwise encounter.
*/

/*
 * Copyright (c) 2007-2009 Roy Marples <roy@marples.name>
 *
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
#include <sys/time.h>

#include <errno.h>
#include <ctype.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <syslog.h>
#include <time.h>
#include <unistd.h>

#include "builtins.h"
#include "einfo.h"
#include "rc-misc.h"

/* usecs to wait while we poll the file existance  */
#define WAIT_INTERVAL	20000000
#define ONE_SECOND      690000000

/* Applet is first parsed in rc.c - no point in doing it again */
extern const char *applet;

static int
syslog_decode(char *name, CODE *codetab)
{
	CODE *c;

	if (isdigit((unsigned char)*name))
		return atoi(name);

	for (c = codetab; c->c_name; c++)
		if (! strcasecmp(name, c->c_name))
			return c->c_val;

	return -1;
}

static int
do_e(int argc, char **argv)
{
	int retval = EXIT_SUCCESS;
	int i;
	size_t l = 0;
	char *message = NULL;
	char *p;
	int level = 0;
	struct timespec ts;
	struct timeval stop, now;
	int (*e) (const char *, ...) EINFO_PRINTF(1, 2) = NULL;
	int (*ee) (int, const char *, ...) EINFO_PRINTF(2, 3) = NULL;

	/* Punt applet */
	argc--;
	argv++;

	if (strcmp(applet, "eval_ecolors") == 0) {
		printf("GOOD='%s'\nWARN='%s'\nBAD='%s'\nHILITE='%s'\nBRACKET='%s'\nNORMAL='%s'\n",
		    ecolor(ECOLOR_GOOD),
		    ecolor(ECOLOR_WARN),
		    ecolor(ECOLOR_BAD),
		    ecolor(ECOLOR_HILITE),
		    ecolor(ECOLOR_BRACKET),
		    ecolor(ECOLOR_NORMAL));
		exit(EXIT_SUCCESS);
	}

	if (argc > 0) {
		if (strcmp(applet, "eend") == 0 ||
		    strcmp(applet, "ewend") == 0 ||
		    strcmp(applet, "veend") == 0 ||
		    strcmp(applet, "vweend") == 0 ||
		    strcmp(applet, "ewaitfile") == 0)
		{
			errno = 0;
			retval = (int)strtoimax(argv[0], &p, 0);
			if (!p || *p != '\0')
				errno = EINVAL;
			if (errno)
				retval = EXIT_FAILURE;
			else {
				argc--;
				argv++;
			}
		} else if (strcmp(applet, "esyslog") == 0 ||
		    strcmp(applet, "elog") == 0) {
			p = strchr(argv[0], '.');
			if (!p ||
			    (level = syslog_decode(p + 1, prioritynames)) == -1)
				eerrorx("%s: invalid log level `%s'", applet, argv[0]);

			if (argc < 3)
				eerrorx("%s: not enough arguments", applet);

			unsetenv("EINFO_LOG");
			setenv("EINFO_LOG", argv[1], 1);

			argc -= 2;
			argv += 2;
		}
	}

	if (strcmp(applet, "ewaitfile") == 0) {
		if (errno)
			eerrorx("%s: invalid timeout", applet);
		if (argc == 0)
			eerrorx("%s: not enough arguments", applet);

		gettimeofday(&stop, NULL);
		/* retval stores the timeout */
		stop.tv_sec += retval;
		ts.tv_sec = 0;
		ts.tv_nsec = WAIT_INTERVAL;
		for (i = 0; i < argc; i++) {
			ebeginv("Waiting for %s", argv[i]);
			for (;;) {
				if (exists(argv[i]))
					break;
				if (nanosleep(&ts, NULL) == -1)
					return EXIT_FAILURE;
				gettimeofday(&now, NULL);
				if (retval <= 0)
					continue;
				if (timercmp(&now, &stop, <))
					continue;
				eendv(EXIT_FAILURE,
				    "timed out waiting for %s", argv[i]);
				return EXIT_FAILURE;
			}
			eendv(EXIT_SUCCESS, NULL);
		}
		return EXIT_SUCCESS;
	}

	if (argc > 0) {
		for (i = 0; i < argc; i++)
			l += strlen(argv[i]) + 1;

		message = xmalloc(l);
		p = message;

		for (i = 0; i < argc; i++) {
			if (i > 0)
				*p++ = ' ';
			l = strlen(argv[i]);
			memcpy(p, argv[i], l);
			p += l;
		}
		*p = 0;
	}

	if (strcmp(applet, "einfo") == 0)
		e = einfo;
	else if (strcmp(applet, "einfon") == 0)
		e = einfon;
	else if (strcmp(applet, "ewarn") == 0)
		e = ewarn;
	else if (strcmp(applet, "ewarnn") == 0)
		e = ewarnn;
	else if (strcmp(applet, "eerror") == 0) {
		e = eerror;
		retval = 1;
	} else if (strcmp(applet, "eerrorn") == 0) {
		e = eerrorn;
		retval = 1;
	} else if (strcmp(applet, "ebegin") == 0)
		e = ebegin;
	else if (strcmp(applet, "eend") == 0)
		ee = eend;
	else if (strcmp(applet, "ewend") == 0)
		ee = ewend;
	else if (strcmp(applet, "esyslog") == 0) {
		elog(retval, "%s", message);
		retval = 0;
	} else if (strcmp(applet, "veinfo") == 0)
		e = einfov;
	else if (strcmp(applet, "veinfon") == 0)
		e = einfovn;
	else if (strcmp(applet, "vewarn") == 0)
		e = ewarnv;
	else if (strcmp(applet, "vewarnn") == 0)
		e = ewarnvn;
	else if (strcmp(applet, "vebegin") == 0)
		e = ebeginv;
	else if (strcmp(applet, "veend") == 0)
		ee = eendv;
	else if (strcmp(applet, "vewend") == 0)
		ee = ewendv;
	else if (strcmp(applet, "eindent") == 0)
		eindent();
	else if (strcmp(applet, "eoutdent") == 0)
		eoutdent();
	else if (strcmp(applet, "veindent") == 0)
		eindentv();
	else if (strcmp(applet, "veoutdent") == 0)
		eoutdentv();
	else {
		eerror("%s: unknown applet", applet);
		retval = EXIT_FAILURE;
	}

	if (message) {
		if (e)
			e("%s", message);
		else if (ee)
			ee(retval, "%s", message);
	} else {
		if (e)
			e(NULL);
		else if (ee)
			ee(retval, NULL);
	}

	free(message);
	return retval;
}

static const struct {
	const char * const name;
	RC_SERVICE bit;
} service_bits[] = {
	{ "service_started",     RC_SERVICE_STARTED,     },
	{ "service_stopped",     RC_SERVICE_STOPPED,     },
	{ "service_inactive",    RC_SERVICE_INACTIVE,    },
	{ "service_starting",    RC_SERVICE_STARTING,    },
	{ "service_stopping",    RC_SERVICE_STOPPING,    },
	{ "service_hotplugged",  RC_SERVICE_HOTPLUGGED,  },
	{ "service_wasinactive", RC_SERVICE_WASINACTIVE, },
	{ "service_failed",      RC_SERVICE_FAILED,      },
};

static RC_SERVICE
lookup_service_state(const char *service)
{
	size_t i;
	for (i = 0; i < ARRAY_SIZE(service_bits); ++i)
		if (!strcmp(service, service_bits[i].name))
			return service_bits[i].bit;
	return 0;
}

static int
do_service(int argc, char **argv)
{
	bool ok = false;
	char *service;
	char *exec;
	int idx;
	RC_SERVICE state, bit;

	if (argc > 1)
		service = argv[1];
	else
		service = getenv("RC_SVCNAME");

	if (service == NULL || *service == '\0')
		eerrorx("%s: no service specified", applet);

	state = rc_service_state(service);
	bit = lookup_service_state(applet);
	if (bit) {
		ok = (state & bit);
	} else if (strcmp(applet, "service_started_daemon") == 0) {
		service = getenv("RC_SVCNAME");
		exec = argv[1];
		if (argc > 3) {
			service = argv[1];
			exec = argv[2];
			sscanf(argv[3], "%d", &idx);
		} else if (argc == 3) {
			if (sscanf(argv[2], "%d", &idx) != 1) {
				service = argv[1];
				exec = argv[2];
			}
		}
		ok = rc_service_started_daemon(service, exec, NULL, idx);

	} else if (strcmp(applet, "service_crashed") == 0) {
		ok = (_rc_can_find_pids() &&
		    rc_service_daemons_crashed(service) &&
		    errno != EACCES);
	} else
		eerrorx("%s: unknown applet", applet);

	return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

static int
do_mark_service(int argc, char **argv)
{
	bool ok = false;
	char *svcname = getenv("RC_SVCNAME");
	char *service = NULL;
	char *openrc_pid;
	/* char *mtime; */
	pid_t pid;
	RC_SERVICE bit;
	/* size_t l; */

	if (argc > 1)
		service = argv[1];
	else
		service = svcname;

	if (service == NULL || *service == '\0')
		eerrorx("%s: no service specified", applet);

	if (!strncmp(applet, "mark_", 5) &&
	    (bit = lookup_service_state(applet + 5)))
		ok = rc_service_mark(service, bit);
	else
		eerrorx("%s: unknown applet", applet);

	/* If we're marking ourselves then we need to inform our parent
	   openrc-run process so they do not mark us based on our exit code */
	/*
	 * FIXME: svcname and service are almost always equal except called from a
	 * shell with just argv[1] - So that doesn't seem to do what Roy initially
	 * expected.
	 * See 20120424041423.GA23657@odin.qasl.de (Tue, 24 Apr 2012 06:14:23 +0200,
	 * openrc@gentoo.org).
	 */
	if (ok && svcname && strcmp(svcname, service) == 0) {
		openrc_pid = getenv("RC_OPENRC_PID");
		if (openrc_pid && sscanf(openrc_pid, "%d", &pid) == 1)
			if (kill(pid, SIGHUP) != 0)
				eerror("%s: failed to signal parent %d: %s",
				    applet, pid, strerror(errno));

		/* Remove the exclusive time test. This ensures that it's not
		   in control as well */
		/*
		l = strlen(RC_SVCDIR "/exclusive") + strlen(svcname) +
		    strlen(openrc_pid) + 4;
		mtime = xmalloc(l);
		snprintf(mtime, l, RC_SVCDIR "/exclusive/%s.%s",
		    svcname, openrc_pid);
		if (exists(mtime) && unlink(mtime) != 0)
			eerror("%s: unlink: %s", applet, strerror(errno));
		free(mtime);
		*/
	}

	return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

static int
do_value(int argc, char **argv)
{
	bool ok = false;
	char *service = getenv("RC_SVCNAME");
	char *option;

	if (service == NULL)
		eerrorx("%s: no service specified", applet);

	if (argc < 2 || ! argv[1] || *argv[1] == '\0')
		eerrorx("%s: no option specified", applet);

	if (strcmp(applet, "service_get_value") == 0 ||
	    strcmp(applet, "get_options") == 0)
	{
		option = rc_service_value_get(service, argv[1]);
		if (option) {
			printf("%s", option);
			free(option);
			ok = true;
		}
	} else if (strcmp(applet, "service_set_value") == 0 ||
	    strcmp(applet, "save_options") == 0)
		ok = rc_service_value_set(service, argv[1], argv[2]);
	else
		eerrorx("%s: unknown applet", applet);

	return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

static int
shell_var(int argc, char **argv)
{
	int i;
	char *p;
	int c;

	for (i = 1; i < argc; i++) {
		p = argv[i];
		if (i != 1)
			putchar(' ');
		while (*p) {
			c = (unsigned char)*p++;
			if (! isalnum(c))
				c = '_';
			putchar(c);
		}
	}
	putchar('\n');
	return EXIT_SUCCESS;
}

static int
is_older_than(int argc, char **argv)
{
	int i;

	if (argc < 3)
		return EXIT_FAILURE;

	/* This test is perverted - historically the baselayout function
	 * returns 0 on *failure*, which is plain wrong */
	for (i = 2; i < argc; ++i)
		if (!rc_newer_than(argv[1], argv[i], NULL, NULL))
			return EXIT_SUCCESS;

	return EXIT_FAILURE;
}

static int
is_newer_than(int argc, char **argv)
{
	int i;

	if (argc < 3)
		return EXIT_FAILURE;

	/* This test is correct as it's not present in baselayout */
	for (i = 2; i < argc; ++i)
		if (!rc_newer_than(argv[1], argv[i], NULL, NULL))
			return EXIT_FAILURE;

	return EXIT_SUCCESS;
}

static int
is_runlevel_start(_unused int argc, _unused char **argv)
{
	return rc_runlevel_starting() ? 0 : 1;
}

static int
is_runlevel_stop(_unused int argc, _unused char **argv)
{
	return rc_runlevel_stopping() ? 0 : 1;
}

static int
rc_abort(_unused int argc, _unused char **argv)
{
	const char *p = getenv("RC_PID");
	int pid;

	if (p && sscanf(p, "%d", &pid) == 1) {
		if (kill(pid, SIGUSR1) != 0)
			eerrorx("rc-abort: failed to signal parent %d: %s",
			    pid, strerror(errno));
		return EXIT_SUCCESS;
	}

	return EXIT_FAILURE;
}

static const struct {
	const char * const name;
	int (* const applet)(int argc, char **argv);
} applets[] = {
#define A(a) { #a, a }
	A(fstabinfo),
	A(mountinfo),
	{ "openrc-run",           openrc_run,         },
	{ "rc-depend",           rc_depend,         },
	{ "rc-service",          rc_service,        },
	{ "rc-status",           rc_status,         },
	{ "rc-update",           rc_update,         },
	{ "service",             rc_service,        },
	{ "update-rc",           rc_update,         },
	A(runscript),
	{ "start-stop-daemon",   start_stop_daemon, },
	A(checkpath),
	A(swclock),
	A(shell_var),
	A(is_older_than),
	A(is_newer_than),
	A(is_runlevel_start),
	A(is_runlevel_stop),
	{ "rc-abort",            rc_abort,          },
	/* These are purely for init scripts and do not make sense as
	 * anything else */
	{ "service_get_value",   do_value,          },
	{ "service_set_value",   do_value,          },
	{ "get_options",         do_value,          },
	{ "save_options",        do_value,          },
#undef A
};

void
run_applets(int argc, char **argv)
{
	size_t i;

	/*
	 * The "rc" applet is deprecated and should be referred to as
	 * "openrc", so output a warning.
	 */
	if (strcmp(applet, "rc") == 0)
		ewarnv("The 'rc' applet is deprecated; please use 'openrc' instead.");
	/* Bug 351712: We need an extra way to explicitly select an applet OTHER
	 * than trusting argv[0], as argv[0] is not going to be the applet value if
	 * we are doing SELinux context switching. For this, we allow calls such as
	 * 'rc --applet APPLET', and shift ALL of argv down by two array items. */
	if ((strcmp(applet, "rc") == 0 || strcmp(applet, "openrc") == 0) &&
		argc >= 3 &&
		(strcmp(argv[1],"--applet") == 0 || strcmp(argv[1], "-a") == 0)) {
		applet = argv[2];
		argv += 2;
		argc -= 2;
	}

	for (i = 0; i < ARRAY_SIZE(applets); ++i)
		if (!strcmp(applet, applets[i].name))
			exit(applets[i].applet(argc, argv));

	if (applet[0] == 'e' || (applet[0] == 'v' && applet[1] == 'e'))
		exit(do_e(argc, argv));

	if (strncmp(applet, "service_", strlen("service_")) == 0)
		exit(do_service(argc, argv));

	if (strncmp(applet, "mark_service_", strlen("mark_service_")) == 0)
		exit(do_mark_service(argc, argv));

	if (strcmp(applet, "rc") != 0 && strcmp(applet, "openrc") != 0)
		eerrorx("%s: unknown applet", applet);
}
