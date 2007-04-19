/*
 * runscript.c
 * Handle launching of Gentoo init scripts.
 *
 * Copyright 1999-2007 Gentoo Foundation
 * Distributed under the terms of the GNU General Public License v2
 */

#define APPLET "runscript"

#include <sys/types.h>
#include <sys/signal.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <dlfcn.h>
#include <errno.h>
#include <getopt.h>
#include <libgen.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "einfo.h"
#include "rc.h"
#include "rc-misc.h"
#include "rc-plugin.h"
#include "strlist.h"

#define RCSCRIPT_HELP   RC_LIBDIR "/sh/rc-help.sh"
#define SELINUX_LIB     RC_LIBDIR "/runscript_selinux.so"

static char *applet = NULL;
static char *exclusive = NULL;
static char *mtime_test = NULL;
static rc_depinfo_t *deptree = NULL;
static char **services = NULL;
static char **svclist = NULL;
static char **tmplist = NULL;
static char **providelist = NULL;
static char **types = NULL;
static char **restart_services = NULL;
static char **need_services = NULL;
static char **env = NULL;
static char *mycmd = NULL;
static char *myarg1 = NULL;
static char *myarg2 = NULL;
static char *tmp = NULL;
static char *softlevel = NULL;
static bool sighup = false;
static char *ibsave = NULL;
static bool in_background = false;
static rc_hook_t hook_out = 0;

extern char **environ;

#ifdef __linux__
static void (*selinux_run_init_old) (void);
static void (*selinux_run_init_new) (int argc, char **argv);

void setup_selinux (int argc, char **argv);
#endif

#ifdef __linux__
void setup_selinux (int argc, char **argv)
{
	void *lib_handle = NULL;

	lib_handle = dlopen (SELINUX_LIB, RTLD_NOW | RTLD_GLOBAL);
	if (lib_handle) {
		/* FIXME: the below code generates the warning
		   ISO C forbids assignment between function pointer and 'void *'
		   which sucks ass
http://www.opengroup.org/onlinepubs/009695399/functions/dlsym.html */
		selinux_run_init_old = dlsym (lib_handle, "selinux_runscript");
		selinux_run_init_new = dlsym (lib_handle, "selinux_runscript2");

		/* Use new run_init if it rc_exists, else fall back to old */
		if (selinux_run_init_new)
			selinux_run_init_new (argc, argv);
		else if (selinux_run_init_old)
			selinux_run_init_old ();
		else
			/* This shouldnt happen... probably corrupt lib */
			eerrorx ("run_init is missing from runscript_selinux.so!");
	}
}
#endif

static void handle_signal (int sig)
{
	pid_t pid;
	int status;
	int serrno = errno;
	char signame[10] = { '\0' };

	switch (sig) {
		case SIGHUP:
			sighup = true;
			break;

		case SIGCHLD:
			do {
				pid = waitpid (-1, &status, WNOHANG);
				if (pid < 0) {
					if (errno != ECHILD)
						eerror ("waitpid: %s", strerror (errno));
					return;
				}
			} while (! WIFEXITED (status) && ! WIFSIGNALED (status));
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

		default:
			eerror ("%s: caught unknown signal %d", applet, sig);
	}

	/* Restore errno */
	errno = serrno;
}

static time_t get_mtime (const char *pathname, bool follow_link)
{
	struct stat buf;
	int retval;

	if (! pathname)
		return (0);

	retval = follow_link ? stat (pathname, &buf) : lstat (pathname, &buf);
	if (! retval)
		return (buf.st_mtime);

	errno = 0;
	return (0);
}

static bool in_control ()
{
	char *path;
	time_t mtime;
	const char *tests[] = { "starting", "started", "stopping",
		"inactive", "wasinactive", NULL };
	int i = 0;

	if (sighup)
		return (false);

	if (! mtime_test || ! rc_exists (mtime_test))
		return (false);

	if (rc_service_state (applet, rc_service_stopped))
		return (false);

	if (! (mtime = get_mtime (mtime_test, false)))
		return (false);

	while (tests[i]) {
		path = rc_strcatpaths (RC_SVCDIR, tests[i], applet, (char *) NULL);
		if (rc_exists (path)) {
			time_t m = get_mtime (path, false);
			if (mtime < m && m != 0) {
				free (path);
				return (false);
			}
		}
		free (path);
		i++;
	}

	return (true);
}

static void uncoldplug (char *service)
{
	char *cold = rc_strcatpaths (RC_SVCDIR "coldplugged", basename (service),
								 (char *) NULL);
	if (rc_exists (cold) && unlink (cold) != 0)
		eerror ("%s: unlink `%s': %s", applet, cold, strerror (errno));
	free (cold);
}

static void cleanup (void)
{
	/* Flush our buffered output if any */
	eflush ();

	if (hook_out)
		rc_plugin_run (hook_out, applet);
	rc_plugin_unload ();

	if (deptree)
		rc_free_deptree (deptree);
	if (services)
		rc_strlist_free (services);
	if (types)
		rc_strlist_free (types);
	if (svclist)
		rc_strlist_free (svclist);
	if (providelist)
		rc_strlist_free (providelist);
	if (restart_services)
		rc_strlist_free (restart_services);
	if (need_services)
		rc_strlist_free (need_services);
	if (tmplist)
		rc_strlist_free (tmplist);
	if (mycmd)
		free (mycmd);
	if (myarg1)
		free (myarg1);
	if (myarg2)
		free (myarg2);
	if (ibsave)
		free (ibsave);

	if (in_control ()) {
		if (rc_service_state (applet, rc_service_stopping)) {
			/* If the we're shutting down, do it cleanly */
			if ((softlevel &&
				 rc_runlevel_stopping () &&
				 (strcmp (softlevel, RC_LEVEL_SHUTDOWN) == 0 ||
				  strcmp (softlevel, RC_LEVEL_REBOOT) == 0)))
				rc_mark_service (applet, rc_service_stopped);
			else if (rc_service_state (applet, rc_service_wasinactive))
				rc_mark_service (applet, rc_service_inactive);
			else
				rc_mark_service (applet, rc_service_started);
		}
		else if (rc_service_state (applet, rc_service_starting))
		{
			if (rc_service_state (applet, rc_service_wasinactive))
				rc_mark_service (applet, rc_service_inactive);
			else 
				rc_mark_service (applet, rc_service_stopped);
		}
		if (exclusive && rc_exists (exclusive))
			unlink (exclusive);
	}

	if (env)
		rc_strlist_free (env);

	if (mtime_test)
	{
		unlink (mtime_test);
		free (mtime_test);
	}
	if (exclusive)
		free (exclusive);

	if (applet)
		free (applet);
}

static bool svc_exec (const char *service, const char *arg1, const char *arg2)
{
	int status = 0;
	pid_t pid;

	/* We need to disable our child signal handler now so we block
	   until our script returns. */
	signal (SIGCHLD, NULL);

	pid = fork();

	if (pid == -1)
		eerrorx ("%s: fork: %s", service, strerror (errno));
	if (pid == 0) {
		mycmd = rc_xstrdup (service);
		myarg1 = rc_xstrdup (arg1);
		if (arg2)
			myarg2 = rc_xstrdup (arg2);

		if (rc_exists (RC_SVCDIR "runscript.sh")) {
			execl (RC_SVCDIR "runscript.sh", mycmd, mycmd, myarg1, myarg2,
				   (char *) NULL);
			eerrorx ("%s: exec `" RC_SVCDIR "runscript.sh': %s",
					 service, strerror (errno));
		} else {
			execl (RC_LIBDIR "sh/runscript.sh", mycmd, mycmd, myarg1, myarg2,
				   (char *) NULL);
			eerrorx ("%s: exec `" RC_LIBDIR "sh/runscript.sh': %s",
					 service, strerror (errno));
		}
	}

	do {
		if (waitpid (pid, &status, 0) < 0) {
			if (errno != ECHILD)
				eerror ("waitpid: %s", strerror (errno));
			break;
		}
	} while (! WIFEXITED (status) && ! WIFSIGNALED (status));

	/* Done, so restore the signal handler */
	signal (SIGCHLD, handle_signal);

	if (WIFEXITED (status))
		return (WEXITSTATUS (status) ? false : true);

	return (false);
}

static rc_service_state_t svc_status (const char *service)
{
	char status[10];
	int (*e) (const char *fmt, ...) = &einfo;

	rc_service_state_t retval = rc_service_stopped;

	if (rc_service_state (service, rc_service_stopping)) {
		snprintf (status, sizeof (status), "stopping");
		e = &ewarn;
		retval = rc_service_stopping;
	} else if (rc_service_state (service, rc_service_starting)) {
		snprintf (status, sizeof (status), "starting");
		e = &ewarn;
		retval = rc_service_starting;
	} else if (rc_service_state (service, rc_service_inactive)) {
		snprintf (status, sizeof (status), "inactive");
		e = &ewarn;
		retval = rc_service_inactive;
	} else if (rc_service_state (service, rc_service_crashed)) {
		snprintf (status, sizeof (status), "crashed");
		e = &eerror;
		retval = rc_service_crashed;
	} else if (rc_service_state (service, rc_service_started)) {
		snprintf (status, sizeof (status), "started");
		retval = rc_service_started;
	} else
		snprintf (status, sizeof (status), "stopped");

	e ("status: %s", status);
	return (retval);
}

static void make_exclusive (const char *service)
{
	char *path;
	int i;

	/* We create a fifo so that other services can wait until we complete */
	if (! exclusive)
		exclusive = rc_strcatpaths (RC_SVCDIR, "exclusive", applet, (char *) NULL);

	if (mkfifo (exclusive, 0600) != 0 && errno != EEXIST &&
		(errno != EACCES || geteuid () == 0))
		eerrorx ("%s: unable to create fifo `%s': %s",
				 applet, exclusive, strerror (errno));

	path = rc_strcatpaths (RC_SVCDIR, "exclusive", applet, (char *) NULL);
	i = strlen (path) + 16;
	mtime_test = rc_xmalloc (sizeof (char *) * i);
	snprintf (mtime_test, i, "%s.%d", path, getpid ());
	free (path);

	if (rc_exists (mtime_test) && unlink (mtime_test) != 0) {
		eerror ("%s: unlink `%s': %s",
				applet, mtime_test, strerror (errno));
		free (mtime_test);
		mtime_test = NULL;
		return;
	}

	if (symlink (service, mtime_test) != 0) {
		eerror ("%s: symlink `%s' to `%s': %s",
				applet, service, mtime_test, strerror (errno));
		free (mtime_test);
		mtime_test = NULL;
	}
}

static void unlink_mtime_test ()
{
	if (unlink (mtime_test) != 0)
		eerror ("%s: unlink `%s': %s", applet, mtime_test, strerror (errno));
	free (mtime_test);
	mtime_test = NULL;
}

static void get_started_services ()
{
	char *service;
	int i;

	rc_strlist_free (tmplist);
	tmplist = rc_services_in_state (rc_service_inactive);

	rc_strlist_free (restart_services);
	restart_services = rc_services_in_state (rc_service_started);

	STRLIST_FOREACH (tmplist, service, i)
		restart_services = rc_strlist_addsort (restart_services, service);

	rc_strlist_free (tmplist);
	tmplist = NULL;
}

static void svc_start (const char *service, bool deps)
{
	bool started;
	bool background = false;
	char *svc;
	char *svc2;
	int i;
	int j;
	int depoptions = RC_DEP_TRACE;

	rc_plugin_run (rc_hook_service_start_in, applet);
	hook_out = rc_hook_service_start_out;

	if (rc_is_env ("RC_STRICT_DEPEND", "yes"))
		depoptions |= RC_DEP_STRICT;

	if (rc_is_env ("IN_HOTPLUG", "1") || in_background) {
		if (! rc_service_state (service, rc_service_inactive) &&
			! rc_service_state (service, rc_service_stopped))
			exit (EXIT_FAILURE);
		background = true;
	}

	if (rc_service_state (service, rc_service_started))
		ewarnx ("WARNING: %s has already been started", applet);
	else if (rc_service_state (service, rc_service_starting))
		ewarnx ("WARNING: %s is already starting", applet);
	else if (rc_service_state (service, rc_service_stopping))
		ewarnx ("WARNING: %s is stopping", applet);
	else if (rc_service_state (service, rc_service_inactive) && ! background)
		ewarnx ("WARNING: %s has already started, but is inactive", applet);

	if (! rc_mark_service (service, rc_service_starting))
		eerrorx ("ERROR: %s has been started by something else", applet);

	make_exclusive (service);

	if (deps) {
		if (! deptree && ((deptree = rc_load_deptree ()) == NULL))
			eerrorx ("failed to load deptree");

		rc_strlist_free (types);
		types = rc_strlist_add (NULL, "broken");
		rc_strlist_free (svclist);
		svclist = rc_strlist_add (NULL, applet);
		rc_strlist_free (services);
		services = rc_get_depends (deptree, types, svclist, softlevel, 0);
		if (services) {
			eerrorn ("ERROR: `%s' needs ", applet);
			STRLIST_FOREACH (services, svc, i) {
				if (i > 0)
					fprintf (stderr, ", ");
				fprintf (stderr, "%s", svc);
			}
			exit (EXIT_FAILURE);
		}
		rc_strlist_free (services);
		services = NULL;

		rc_strlist_free (types);
		types = rc_strlist_add (NULL, "ineed");
		rc_strlist_free (need_services);
		need_services = rc_get_depends (deptree, types, svclist,
										softlevel, depoptions);
		types = rc_strlist_add (types, "iuse");
		if (! rc_runlevel_starting ()) {
			services = rc_get_depends (deptree, types, svclist,
									   softlevel, depoptions);
			STRLIST_FOREACH (services, svc, i)
				if (rc_service_state (svc, rc_service_stopped))
					rc_start_service (svc);

			rc_strlist_free (services);
		}

		/* Now wait for them to start */
		types = rc_strlist_add (types, "iafter");
		services = rc_get_depends (deptree, types, svclist,
								   softlevel, depoptions);

		/* We use tmplist to hold our scheduled by list */
		rc_strlist_free (tmplist);
		tmplist = NULL;

		STRLIST_FOREACH (services, svc, i) {
			if (rc_service_state (svc, rc_service_started))
				continue;
			if (! rc_wait_service (svc))
				eerror ("%s: timed out waiting for %s", applet, svc);
			if (rc_service_state (svc, rc_service_started))
				continue;

			STRLIST_FOREACH (need_services, svc2, j)
				if (strcmp (svc, svc2) == 0) {
					if (rc_service_state (svc, rc_service_inactive) ||
						rc_service_state (svc, rc_service_wasinactive))
						tmplist = rc_strlist_add (tmplist, svc);
					else
						eerrorx ("ERROR: cannot start %s as %s would not start",
								 applet, svc);
				}
		}

		if (tmplist) {
			int n = 0;
			int len = 0;
			char *p;

			/* Set the state now, then unlink our exclusive so that
			   our scheduled list is preserved */
			rc_mark_service (service, rc_service_stopped);
			unlink_mtime_test ();

			rc_strlist_free (types);
			types = rc_strlist_add (NULL, "iprovide");
			STRLIST_FOREACH (tmplist, svc, i) {
				rc_schedule_start_service (svc, service);

				rc_strlist_free (svclist);
				svclist = rc_strlist_add (NULL, svc);
				rc_strlist_free (providelist);
				providelist = rc_get_depends (deptree, types, svclist,
											  softlevel, depoptions);
				STRLIST_FOREACH (providelist, svc2, j) 
					rc_schedule_start_service (svc2, service);

				len += strlen (svc) + 2;
				n++;
			}

			len += 5;
			tmp = rc_xmalloc (sizeof (char *) * len);
			p = tmp;
			STRLIST_FOREACH (tmplist, svc, i) {
				if (i > 1) {
					if (i == n - 1)
						p += snprintf (p, len, " or ");
					else
						p += snprintf (p, len, ", ");
				}
				p += snprintf (p, len, "%s", svc);
			}
			ewarnx ("WARNING: %s is scheduled to start when %s has started",
					applet, tmp);
		}

		rc_strlist_free (services);
		services = NULL;
		rc_strlist_free (types);
		types = NULL;
		rc_strlist_free (svclist);
		svclist = NULL;
	}

	if (ibsave)
		setenv ("IN_BACKGROUND", ibsave, 1);
	rc_plugin_run (rc_hook_service_start_now, applet);
	started = svc_exec (service, "start", NULL);
	if (ibsave)
		unsetenv ("IN_BACKGROUND");

	if (in_control ()) {
		if (! started) {
			if (rc_service_state (service, rc_service_wasinactive))
				rc_mark_service (service, rc_service_inactive);
			else {
				rc_mark_service (service, rc_service_stopped);
				if (rc_runlevel_starting ())
					rc_mark_service (service, rc_service_failed);
			}
			rc_plugin_run (rc_hook_service_start_done, applet);
			eerrorx ("ERROR: %s failed to start", applet);
		}
		rc_mark_service (service, rc_service_started);
		unlink_mtime_test ();
		rc_plugin_run (rc_hook_service_start_done, applet);
	} else {
		rc_plugin_run (rc_hook_service_start_done, applet);
		if (rc_service_state (service, rc_service_inactive))
			ewarnx ("WARNING: %s has started, but is inactive", applet);
		else
			ewarnx ("WARNING: %s not under our control, aborting", applet);
	}

	/* Now start any scheduled services */
	rc_strlist_free (services);
	services = rc_services_scheduled (service);
	STRLIST_FOREACH (services, svc, i)
		if (rc_service_state (svc, rc_service_stopped))
			rc_start_service (svc);
	rc_strlist_free (services);
	services = NULL;

	/* Do the same for any services we provide */
	rc_strlist_free (types);
	types = rc_strlist_add (NULL, "iprovide");
	rc_strlist_free (svclist);
	svclist = rc_strlist_add (NULL, applet);
	rc_strlist_free (tmplist);
	tmplist = rc_get_depends (deptree, types, svclist, softlevel, depoptions);

	STRLIST_FOREACH (tmplist, svc2, j) {
		rc_strlist_free (services);
		services = rc_services_scheduled (svc2);
		STRLIST_FOREACH (services, svc, i)
			if (rc_service_state (svc, rc_service_stopped))
				rc_start_service (svc);
	}

	hook_out = 0;
	rc_plugin_run (rc_hook_service_start_out, applet);
}

static void svc_stop (const char *service, bool deps)
{
	bool stopped;

	hook_out = rc_hook_service_stop_out;

	if (rc_runlevel_stopping () &&
		rc_service_state (service, rc_service_failed))
		exit (EXIT_FAILURE);

	if (rc_is_env ("IN_HOTPLUG", "1") || in_background)
		if (! rc_service_state (service, rc_service_started) && 
			! rc_service_state (service, rc_service_inactive))
			exit (EXIT_FAILURE);

	if (rc_service_state (service, rc_service_stopped))
		ewarnx ("WARNING: %s is already stopped", applet);
	else if (rc_service_state (service, rc_service_stopping))
		ewarnx ("WARNING: %s is already stopping", applet);

	if (! rc_mark_service (service, rc_service_stopping))
		eerrorx ("ERROR: %s has been stopped by something else", applet);

	make_exclusive (service);

	if (! rc_runlevel_stopping () &&
		rc_service_in_runlevel (service, RC_LEVEL_BOOT))
		ewarn ("WARNING: you are stopping a boot service");

	if (deps || ! rc_service_state (service, rc_service_wasinactive)) {
		int depoptions = RC_DEP_TRACE;
		char *svc;
		int i;

		if (rc_is_env ("RC_STRICT_DEPEND", "yes"))
			depoptions |= RC_DEP_STRICT;

		if (! deptree && ((deptree = rc_load_deptree ()) == NULL))
			eerrorx ("failed to load deptree");

		rc_strlist_free (types);
		types = rc_strlist_add (NULL, "needsme");
		rc_strlist_free (svclist);
		svclist = rc_strlist_add (NULL, applet);
		rc_strlist_free (tmplist);
		tmplist = NULL;
		rc_strlist_free (services);
		services = rc_get_depends (deptree, types, svclist,
								   softlevel, depoptions);
		rc_strlist_reverse (services);
		STRLIST_FOREACH (services, svc, i) {
			if (rc_service_state (svc, rc_service_started) || 
				rc_service_state (svc, rc_service_inactive))
			{
				rc_wait_service (svc);
				if (rc_service_state (svc, rc_service_started) || 
					rc_service_state (svc, rc_service_inactive))
				{
					rc_stop_service (svc);
					tmplist = rc_strlist_add (tmplist, svc);
				}
			}
		}
		rc_strlist_free (services);
		services = NULL;

		STRLIST_FOREACH (tmplist, svc, i) {
			if (rc_service_state (svc, rc_service_stopped))
				continue;

			/* We used to loop 3 times here - maybe re-do this if needed */
			rc_wait_service (svc);
			if (! rc_service_state (svc, rc_service_stopped)) {
				if (rc_runlevel_stopping ())
					rc_mark_service (svc, rc_service_failed);
				eerrorx ("ERROR:  cannot stop %s as %s is still up",
						 applet, svc);
			}
		}
		rc_strlist_free (tmplist);
		tmplist = NULL;

		/* We now wait for other services that may use us and are stopping
		   This is important when a runlevel stops */
		types = rc_strlist_add (types, "usesme");
		types = rc_strlist_add (types, "ibefore");
		services = rc_get_depends (deptree, types, svclist,
								   softlevel, depoptions);
		STRLIST_FOREACH (services, svc, i) {
			if (rc_service_state (svc, rc_service_stopped))
				continue;
			rc_wait_service (svc);
		}

		rc_strlist_free (services);
		services = NULL;
		rc_strlist_free (types);
		types = NULL;
	}

	if (ibsave)
		setenv ("IN_BACKGROUND", ibsave, 1);
	rc_plugin_run (rc_hook_service_stop_now, applet);
	stopped = svc_exec (service, "stop", NULL);
	if (ibsave)
		unsetenv ("IN_BACKGROUND");

	if (! in_control ()) {
		rc_plugin_run (rc_hook_service_stop_done, applet);
		ewarnx ("WARNING: %s not under our control, aborting", applet);
	}

	if (! stopped) {
		if (rc_service_state (service, rc_service_wasinactive))
			rc_mark_service (service, rc_service_inactive);
		else
			rc_mark_service (service, rc_service_started);
		rc_plugin_run (rc_hook_service_stop_done, applet);
		eerrorx ("ERROR: %s failed to stop", applet);
	}

	if (in_background)
		rc_mark_service (service, rc_service_inactive);
	else
		rc_mark_service (service, rc_service_stopped);

	unlink_mtime_test ();
	rc_plugin_run (rc_hook_service_stop_done, applet);
	hook_out = 0;
	rc_plugin_run (rc_hook_service_stop_out, applet);
}

static void svc_restart (const char *service, bool deps)
{
	char *svc;
	int i;
	bool inactive = false;

	/* This is hairly and a better way needs to be found I think!
	   The issue is this - openvpn need net and dns. net can restart
	   dns via resolvconf, so you could have openvpn trying to restart dnsmasq
	   which in turn is waiting on net which in turn is waiting on dnsmasq.
	   The work around is for resolvconf to restart it's services with --nodeps
	   which means just that. The downside is that there is a small window when
	   our status is invalid.
	   One workaround would be to introduce a new status, or status locking. */
	if (! deps) {
		if (rc_service_state (service, rc_service_started) ||
			rc_service_state (service, rc_service_inactive))
			svc_exec (service, "stop", "start");
		else
			svc_exec (service, "start", NULL);
		return;
	}

	if (! rc_service_state (service, rc_service_stopped)) {
		get_started_services ();
		svc_stop (service, deps);

		/* Flush our buffered output if any */
		eflush ();
	}

	svc_start (service, deps);

	inactive = rc_service_state (service, rc_service_inactive);
	if (! inactive)
		inactive = rc_service_state (service, rc_service_wasinactive);

	if (inactive ||
		rc_service_state (service, rc_service_starting) ||
		rc_service_state (service, rc_service_started))
	{
		STRLIST_FOREACH (restart_services, svc, i) {
			if (rc_service_state (svc, rc_service_stopped)) {
				if (inactive) {
					rc_schedule_start_service (service, svc);
					ewarn ("WARNING: %s is scheduled to started when %s has started",
						   svc, applet);
				} else
					rc_start_service (svc);
			}
		}
	}
}

#define getoptstring "dCDNqvh"
static struct option longopts[] = {
	{ "debug",      0, NULL, 'd'},
	{ "nocolor",    0, NULL, 'C'},
	{ "nocolour",   0, NULL, 'C'},
	{ "nodeps",     0, NULL, 'D'},
	{ "quiet",      0, NULL, 'q'},
	{ "verbose",    0, NULL, 'v'},
	{ "help",       0, NULL, 'h'},
	{ NULL,         0, NULL, 0}
};
// #include "_usage.c"

int main (int argc, char **argv)
{
	char *service = argv[1];
	int i;
	bool deps = true;
	bool doneone = false;
	char pid[16];
	int retval;
	char c;

	/* Show help if insufficient args */
	if (argc < 3) {
		execl (RCSCRIPT_HELP, RCSCRIPT_HELP, service, (char *) NULL);
		eerrorx ("%s: failed to exec `" RCSCRIPT_HELP "': %s",
				 applet, strerror (errno));
	}

	applet = rc_xstrdup (basename (service));
	atexit (cleanup);

#ifdef __linux__
	/* coldplug events can trigger init scripts, but we don't want to run them
	   until after rc sysinit has completed so we punt them to the boot runlevel */
	if (rc_exists ("/dev/.rcsysinit")) {
		eerror ("%s: cannot run until sysvinit completes", applet);
		if (mkdir ("/dev/.rcboot", 0755) != 0 && errno != EEXIST)
			eerrorx ("%s: mkdir `/dev/.rcboot': %s", applet, strerror (errno));
		tmp = rc_strcatpaths ("/dev/.rcboot", applet, (char *) NULL);
		symlink (service, tmp);
		exit (EXIT_FAILURE);
	}
#endif

	if ((softlevel = getenv ("RC_SOFTLEVEL")) == NULL) {
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
				putenv (p);

			/* We don't free our list as that would be null in environ */
		}

		softlevel = rc_get_runlevel ();

		/* If not called from RC or another service then don't be parallel */
		unsetenv ("RC_PARALLEL_STARTUP");
	}

	setenv ("RC_ELOG", service, 1);
	setenv ("SVCNAME", applet, 1);

	/* Set an env var so that we always know our pid regardless of any
	   subshells the init script may create so that our mark_service_*
	   functions can always instruct us of this change */
	snprintf (pid, sizeof (pid), "%d", (int) getpid ());
	setenv ("RC_RUNSCRIPT_PID", pid, 1);

	if (rc_is_env ("RC_PARALLEL_STARTUP", "yes")) {
		char ebname[PATH_MAX];
		char *eb;

		snprintf (ebname, sizeof (ebname), "%s.%s", applet, pid);
		eb = rc_strcatpaths (RC_SVCDIR "ebuffer", ebname, (char *) NULL);
		setenv ("RC_EBUFFER", eb, 1);
		free (eb);
	}

#ifdef __linux__
	/* Ok, we are ready to go, so setup selinux if applicable */
	setup_selinux (argc, argv);
#endif

	/* Punt the first arg as it's our service name */
	argc--;
	argv++;

	/* Right then, parse any options there may be */
	while ((c = getopt_long (argc, argv, getoptstring,
							 longopts, (int *) 0)) != -1)
		switch (c) {
			case 'd':
				setenv ("RC_DEBUG", "yes", 1);
				break;
			case 'h':
				execl (RCSCRIPT_HELP, RCSCRIPT_HELP, service, (char *) NULL);
				eerrorx ("%s: failed to exec `" RCSCRIPT_HELP "': %s",
						 applet, strerror (errno));
			case 'C':
				setenv ("RC_NOCOLOR", "yes", 1);
				break;
			case 'D':
				deps = false;
				break;
			case 'q':
				setenv ("RC_QUIET", "yes", 1);
				break;
			case 'v':
				setenv ("RC_VERBOSE", "yes", 1);
				break;
			default:
				exit (EXIT_FAILURE);
		}

	/* Save the IN_BACKGROUND env flag so it's ONLY passed to the service
	   that is being called and not any dependents */
	if (getenv ("IN_BACKGROUND")) {
		in_background = rc_is_env ("IN_BACKGROUND", "true");
		ibsave = rc_xstrdup (getenv ("IN_BACKGROUND"));
		unsetenv ("IN_BACKGROUND");
	}

	if (rc_is_env ("IN_HOTPLUG", "1")) {
		if (! rc_is_env ("RC_HOTPLUG", "yes") || ! rc_allow_plug (applet))
			eerrorx ("%s: not allowed to be hotplugged", applet);
	}

	/* Setup a signal handler */
	signal (SIGHUP, handle_signal);
	signal (SIGINT, handle_signal);
	signal (SIGQUIT, handle_signal);
	signal (SIGTERM, handle_signal);
	signal (SIGCHLD, handle_signal);

	/* Load our plugins */
	rc_plugin_load ();

	/* Now run each option */
	retval = EXIT_SUCCESS;
	while (optind < argc) {
		optarg = argv[optind++];

		/* Abort on a sighup here */
		if (sighup)
			exit (EXIT_FAILURE);

		/* Export the command we're running.
		   This is important as we stamp on the restart function now but
		   some start/stop routines still need to behave differently if
		   restarting. */
		unsetenv ("RC_CMD");
		setenv ("RC_CMD", optarg, 1);

		doneone = true;
		if (strcmp (optarg, "conditionalrestart") == 0 ||
			strcmp (optarg, "condrestart") == 0)
		{
			if (rc_service_state (service, rc_service_started))
				svc_restart (service, deps);
		}
		else if (strcmp (optarg, "restart") == 0)
			svc_restart (service, deps);
		else if (strcmp (optarg, "start") == 0)
			svc_start (service, deps);
		else if (strcmp (optarg, "status") == 0) {
			rc_service_state_t r = svc_status (service);
			retval = (int) r;
		} else if (strcmp (optarg, "stop") == 0) {
			if (in_background)
				get_started_services ();

			svc_stop (service, deps);

			if (! in_background &&
				! rc_runlevel_stopping () &&
				rc_service_state (service, rc_service_stopped))
				uncoldplug (applet);

			if (in_background &&
				rc_service_state (service, rc_service_inactive))
			{
				char *svc;
				int j;
				STRLIST_FOREACH (restart_services, svc, j)
					if (rc_service_state (svc, rc_service_stopped))
						rc_schedule_start_service (service, svc);
			}
		} else if (strcmp (optarg, "zap") == 0) {
			einfo ("Manually resetting %s to stopped state", applet);
			rc_mark_service (applet, rc_service_stopped);
			uncoldplug (applet);
		} else if (strcmp (optarg, "help") == 0) {
			execl (RCSCRIPT_HELP, RCSCRIPT_HELP, service, "help", (char *) NULL);
			eerrorx ("%s: failed to exec `" RCSCRIPT_HELP "': %s",
					 applet, strerror (errno));
		}else
			svc_exec (service, optarg, NULL);

		/* Flush our buffered output if any */
		eflush ();

		/* We should ensure this list is empty after an action is done */
		rc_strlist_free (restart_services);
		restart_services = NULL;
	}

	if (! doneone) {
		execl (RCSCRIPT_HELP, RCSCRIPT_HELP, service, (char *) NULL);
		eerrorx ("%s: failed to exec `" RCSCRIPT_HELP "': %s",
				 applet, strerror (errno));
	}

	return (retval);
}
