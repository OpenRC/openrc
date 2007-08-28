/*
 * runscript.c
 * Handle launching of Gentoo init scripts.
 *
 * Copyright 1999-2007 Gentoo Foundation
 * Distributed under the terms of the GNU General Public License v2
 */

#define APPLET "runscript"

#include <sys/select.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <libgen.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "builtins.h"
#include "einfo.h"
#include "rc.h"
#include "rc-misc.h"
#include "rc-plugin.h"
#include "strlist.h"

#define RCSCRIPT_HELP   RC_LIBDIR "/sh/rc-help.sh"
#define SELINUX_LIB     RC_LIBDIR "/runscript_selinux.so"

#define PREFIX_LOCK		RC_SVCDIR "/prefix.lock"

static char *applet = NULL;
static char *service = NULL;
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
static char **use_services = NULL;
static char **env = NULL;
static char *tmp = NULL;
static char *softlevel = NULL;
static bool sighup = false;
static char *ibsave = NULL;
static bool in_background = false;
static rc_hook_t hook_out = 0;
static pid_t service_pid = 0;
static char *prefix = NULL;
static bool prefix_locked = false;

extern char **environ;

#ifdef __linux__
static void (*selinux_run_init_old) (void);
static void (*selinux_run_init_new) (int argc, char **argv);

static void setup_selinux (int argc, char **argv);

static void setup_selinux (int argc, char **argv)
{
	void *lib_handle = NULL;
	
	if (! rc_exists (SELINUX_LIB))
		return;

	lib_handle = dlopen (SELINUX_LIB, RTLD_NOW | RTLD_GLOBAL);
	if (! lib_handle) {
		eerror ("dlopen: %s", dlerror ());
		return;
	}

	selinux_run_init_old = (void (*)(void))
		dlfunc (lib_handle, "selinux_runscript");
	selinux_run_init_new = (void (*)(int, char **))
		dlfunc (lib_handle, "selinux_runscript2");

	/* Use new run_init if it rc_exists, else fall back to old */
	if (selinux_run_init_new)
		selinux_run_init_new (argc, argv);
	else if (selinux_run_init_old)
		selinux_run_init_old ();
	else
		/* This shouldnt happen... probably corrupt lib */
		eerrorx ("run_init is missing from runscript_selinux.so!");

	dlclose (lib_handle);
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
			if (pid == service_pid)
				service_pid = 0;
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
			/* Send the signal to our children too */
			if (service_pid > 0) 
				kill (service_pid, sig);
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

static void uncoldplug ()
{
	char *cold = rc_strcatpaths (RC_SVCDIR, "coldplugged", applet, (char *) NULL);
	if (rc_exists (cold) && unlink (cold) != 0)
		eerror ("%s: unlink `%s': %s", applet, cold, strerror (errno));
	free (cold);
}

static void start_services (char **list) {
	bool inactive;
	char *svc;
	int i;

	if (! list)
		return;

	inactive = rc_service_state (service, rc_service_inactive);
	if (! inactive)
		inactive = rc_service_state (service, rc_service_wasinactive);

	if (inactive ||
		rc_service_state (service, rc_service_starting) ||
		rc_service_state (service, rc_service_started))
	{
		STRLIST_FOREACH (list, svc, i) {
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

static void cleanup (void)
{
	if (! rc_in_plugin) {
		if (prefix_locked)
			unlink (PREFIX_LOCK);
		if (hook_out)
			rc_plugin_run (hook_out, applet);
		if (restart_services)
			start_services (restart_services);
	}

	rc_plugin_unload ();
	rc_free_deptree (deptree);
	rc_strlist_free (services);
	rc_strlist_free (types);
	rc_strlist_free (svclist);
	rc_strlist_free (providelist);
	rc_strlist_free (need_services);
	rc_strlist_free (use_services);
	rc_strlist_free (restart_services);
	rc_strlist_free (tmplist);
	free (ibsave);

	if (! rc_in_plugin && in_control ()) {
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

	rc_strlist_free (env);

	if (mtime_test)
	{
		if (! rc_in_plugin)
			unlink (mtime_test);
		free (mtime_test);
	}
	free (exclusive);
	free (applet);
	free (prefix);
	free (service);
}

static int write_prefix (int fd, const char *buffer, size_t bytes, bool *prefixed) {
	unsigned int i;
	const char *ec;
	const char *ec_normal = ecolor (ecolor_normal);
	ssize_t ret = 0;

	if (fd == fileno (stdout))
		ec = ecolor (ecolor_hilite);
	else
		ec = ecolor (ecolor_bad);

	for (i = 0; i < bytes; i++) {
		/* We don't prefix escape codes, like eend */
		if (buffer[i] == '\033')
			*prefixed = true;

		if (! *prefixed) {
			ret += write (fd, ec, strlen (ec));
			ret += write (fd, prefix, strlen (prefix));
			ret += write (fd, ec_normal, strlen (ec_normal));
			ret += write (fd, "|", 1);
			*prefixed = true;
		}

		if (buffer[i] == '\n')
			*prefixed = false;
		ret += write (fd, buffer + i, 1);
	}

	return (ret);
}

static bool svc_exec (const char *arg1, const char *arg2)
{
	bool execok;
	int stdout_pipes[2];
	int stderr_pipes[2];

	/* Setup our pipes for prefixed output */
	if (prefix) {
		if (pipe (stdout_pipes))
			eerror ("pipe: %s", strerror (errno));
		if (pipe (stderr_pipes))
			eerror ("pipe: %s", strerror (errno));
	}

	/* We need to disable our child signal handler now so we block
	   until our script returns. */
	signal (SIGCHLD, NULL);

	service_pid = vfork();

	if (service_pid == -1)
		eerrorx ("%s: vfork: %s", service, strerror (errno));
	if (service_pid == 0) {
		if (prefix) {
			int flags;

			if (dup2 (stdout_pipes[1], fileno (stdout)) == -1)
				eerror ("dup2 stdout: %s", strerror (errno));
			close (stdout_pipes[0]);
			if (dup2 (stderr_pipes[1], fileno (stderr)) == -1)
				eerror ("dup2 stderr: %s", strerror (errno));
			close (stderr_pipes[0]);

			/* Stop any scripts from inheriting us */
			if ((flags = fcntl (stdout_pipes[1], F_GETFD, 0)) < 0 ||
				fcntl (stdout_pipes[1], F_SETFD, flags | FD_CLOEXEC) < 0)
				eerror ("fcntl: %s", strerror (errno));
			if ((flags = fcntl (stderr_pipes[1], F_GETFD, 0)) < 0 ||
				fcntl (stderr_pipes[1], F_SETFD, flags | FD_CLOEXEC) < 0)
				eerror ("fcntl: %s", strerror (errno));
		}

		if (rc_exists (RC_SVCDIR "/runscript.sh")) {
			execl (RC_SVCDIR "/runscript.sh", service, service, arg1, arg2,
				   (char *) NULL);
			eerror ("%s: exec `" RC_SVCDIR "/runscript.sh': %s",
					service, strerror (errno));
			_exit (EXIT_FAILURE);
		} else {
			execl (RC_LIBDIR "/sh/runscript.sh", service, service, arg1, arg2,
				   (char *) NULL);
			eerror ("%s: exec `" RC_LIBDIR "/sh/runscript.sh': %s",
					service, strerror (errno));
			_exit (EXIT_FAILURE);
		}
	}

	/* Prefix our piped output */
	if (prefix) {
		bool stdout_done = false;
		bool stdout_prefix_shown = false;
		bool stderr_done = false;
		bool stderr_prefix_shown = false;
		char buffer[RC_LINEBUFFER];

		close (stdout_pipes[1]);
		close (stderr_pipes[1]);

		memset (buffer, 0, RC_LINEBUFFER);
		while (! stdout_done && ! stderr_done) {
			fd_set fds;
			int retval;

			FD_ZERO (&fds);
			FD_SET (stdout_pipes[0], &fds);
			FD_SET (stderr_pipes[0], &fds);
			retval = select (MAX (stdout_pipes[0], stderr_pipes[0]) + 1,
							 &fds, 0, 0, 0);
			if (retval < 0) {
				if (errno != EINTR) {
					eerror ("select: %s", strerror (errno));
					break;
				}
			} else if (retval) {
				ssize_t nr;

				/* Wait until we get a lock */
				while (true) {
					struct timeval tv;

					if (mkfifo (PREFIX_LOCK, 0700) == 0) {
						prefix_locked = true;
						break;
					}

					if (errno != EEXIST)
						eerror ("mkfifo `%s': %s\n", PREFIX_LOCK, strerror (errno));
					tv.tv_sec = 0;
					tv.tv_usec = 20000;
					select (0, NULL, NULL, NULL, &tv);
				}

				if (FD_ISSET (stdout_pipes[0], &fds)) {
					if ((nr = read (stdout_pipes[0], buffer,
									sizeof (buffer))) <= 0)
						stdout_done = true;
					else
						write_prefix (fileno (stdout), buffer, nr,
									  &stdout_prefix_shown);
				}

				if (FD_ISSET (stderr_pipes[0], &fds)) {
					if ((nr = read (stderr_pipes[0], buffer,
									sizeof (buffer))) <= 0)
						stderr_done = true;
					else
						write_prefix (fileno (stderr), buffer, nr,
									  &stderr_prefix_shown);
				}

				/* Clear the lock */
				unlink (PREFIX_LOCK);
				prefix_locked = false;
			}
		}

		/* Done now, so close the pipes */
		close(stdout_pipes[0]);
		close(stderr_pipes[0]);
	}

	execok = rc_waitpid (service_pid) == 0 ? true : false;
	service_pid = 0;

	/* Done, so restore the signal handler */
	signal (SIGCHLD, handle_signal);

	return (execok);
}

static rc_service_state_t svc_status ()
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

static void make_exclusive ()
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
	char *svc;
	int i;

	rc_strlist_free (tmplist);
	tmplist = rc_services_in_state (rc_service_inactive);

	rc_strlist_free (restart_services);
	restart_services = rc_services_in_state (rc_service_started);

	STRLIST_FOREACH (tmplist, svc, i)
		restart_services = rc_strlist_addsort (restart_services, svc);

	rc_strlist_free (tmplist);
	tmplist = NULL;
}

static void svc_start (bool deps)
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

	if (rc_is_env ("IN_HOTPLUG", "1") || in_background) {
		if (! rc_service_state (service, rc_service_inactive) &&
			! rc_service_state (service, rc_service_stopped))
			exit (EXIT_FAILURE);
		background = true;
	}

	if (rc_service_state (service, rc_service_started)) {
		ewarn ("WARNING: %s has already been started", applet);
		return;
	} else if (rc_service_state (service, rc_service_starting))
		ewarnx ("WARNING: %s is already starting", applet);
	else if (rc_service_state (service, rc_service_stopping))
		ewarnx ("WARNING: %s is stopping", applet);
	else if (rc_service_state (service, rc_service_inactive) && ! background)
		ewarnx ("WARNING: %s has already started, but is inactive", applet);

	if (! rc_mark_service (service, rc_service_starting))
		eerrorx ("ERROR: %s has been started by something else", applet);

	make_exclusive (service);

	if (rc_is_env ("RC_DEPEND_STRICT", "yes"))
		depoptions |= RC_DEP_STRICT;

	if (rc_runlevel_starting ())
		depoptions |= RC_DEP_START;

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
		rc_strlist_free (use_services);
		use_services = rc_get_depends (deptree, types, svclist,
									   softlevel, depoptions);

		if (! rc_runlevel_starting ()) {
			STRLIST_FOREACH (use_services, svc, i)
				if (rc_service_state (svc, rc_service_stopped)) {
					pid_t pid = rc_start_service (svc);
					if (! rc_is_env ("RC_PARALLEL", "yes"))
						rc_waitpid (pid);
				}
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

			/* Don't wait for services which went inactive but are now in
			 * starting state which we are after */
			if (rc_service_state (svc, rc_service_starting) &&
				rc_service_state(svc, rc_service_wasinactive)) {
				bool use = false;
				STRLIST_FOREACH (use_services, svc2, j)
					if (strcmp (svc, svc2) == 0) {
						use = true;
						break;
					}
				if (! use)
					continue;
			}
			
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
					if (i == n)
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
	started = svc_exec ("start", NULL);
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

static void svc_stop (bool deps)
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

	if (rc_service_state (service, rc_service_stopped)) {
		ewarn ("WARNING: %s is already stopped", applet);
		return;
	} else if (rc_service_state (service, rc_service_stopping))
		ewarnx ("WARNING: %s is already stopping", applet);

	if (! rc_mark_service (service, rc_service_stopping))
		eerrorx ("ERROR: %s has been stopped by something else", applet);

	make_exclusive (service);

	if (! rc_runlevel_stopping () &&
		rc_service_in_runlevel (service, RC_LEVEL_BOOT))
		ewarn ("WARNING: you are stopping a boot service");

	if (deps && ! rc_service_state (service, rc_service_wasinactive)) {
		int depoptions = RC_DEP_TRACE;
		char *svc;
		int i;

		if (rc_is_env ("RC_DEPEND_STRICT", "yes"))
			depoptions |= RC_DEP_STRICT;

		if (rc_runlevel_stopping ())
			depoptions |= RC_DEP_STOP;

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
					pid_t pid = rc_stop_service (svc);
					if (! rc_is_env ("RC_PARALLEL", "yes"))
						rc_waitpid (pid);
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

				if (rc_runlevel_stopping ()) {
					/* If shutting down, we should stop even if a dependant failed */
					if (softlevel &&
						(strcmp (softlevel, RC_LEVEL_SHUTDOWN) == 0 ||
						 strcmp (softlevel, RC_LEVEL_REBOOT) == 0 ||
						 strcmp (softlevel, RC_LEVEL_SINGLE) == 0))
						continue;
					rc_mark_service (service, rc_service_failed);
				}

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
	stopped = svc_exec ("stop", NULL);
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

static void svc_restart (bool deps)
{
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
			svc_exec ("stop", "start");
		else
			svc_exec ("start", NULL);
		return;
	}

	if (! rc_service_state (service, rc_service_stopped)) {
		get_started_services ();
		svc_stop (deps);
	}

	svc_start (deps);
	start_services (restart_services);
	rc_strlist_free (restart_services);
	restart_services = NULL;
}

#include "_usage.h"
#define getoptstring "dDqsv" getoptstring_COMMON 
static struct option longopts[] = {
	{ "debug",      0, NULL, 'd'},
	{ "ifstarted",  0, NULL, 's'},
	{ "nodeps",     0, NULL, 'D'},
	{ "quiet",      0, NULL, 'q'},
	{ "verbose",    0, NULL, 'v'},
	longopts_COMMON
	{ NULL,         0, NULL, 0}
};
#include "_usage.c"

int runscript (int argc, char **argv)
{
	int i;
	bool deps = true;
	bool doneone = false;
	char pid[16];
	int retval;
	int opt;
	char *svc;

	/* We need the full path to the service */
	if (*argv[1] == '/')
		service = rc_xstrdup (argv[1]);
	else {
		char pwd[PATH_MAX];
		if (! getcwd (pwd, PATH_MAX))
			eerrorx ("getcwd: %s", strerror (errno));
		service = rc_strcatpaths (pwd, argv[1], (char *) NULL);
	}

	applet = rc_xstrdup (basename (service));
	atexit (cleanup);

	/* Change dir to / to ensure all init scripts don't use stuff in pwd */
	chdir ("/");
	
	/* Show help if insufficient args */
	if (argc < 3) {
		execl (RCSCRIPT_HELP, RCSCRIPT_HELP, service, (char *) NULL);
		eerrorx ("%s: failed to exec `" RCSCRIPT_HELP "': %s",
				 applet, strerror (errno));
	}

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
	}

	setenv ("RC_ELOG", service, 1);
	setenv ("SVCNAME", applet, 1);

	/* Set an env var so that we always know our pid regardless of any
	   subshells the init script may create so that our mark_service_*
	   functions can always instruct us of this change */
	snprintf (pid, sizeof (pid), "%d", (int) getpid ());
	setenv ("RC_RUNSCRIPT_PID", pid, 1);

	/* eprefix is kinda klunky, but it works for our purposes */
	if (rc_is_env ("RC_PARALLEL", "yes")) {
		int l = 0;
		int ll;

		/* Get the longest service name */
		services = rc_services_in_runlevel (NULL);
		STRLIST_FOREACH (services, svc, i) {
			ll = strlen (svc);
			if (ll > l)
				l = ll;
		}
	
		/* Make our prefix string */
		prefix = rc_xmalloc (sizeof (char *) * l);
		ll = strlen (applet);
		memcpy (prefix, applet, ll);
		memset (prefix + ll, ' ', l - ll);
		memset (prefix + l, 0, 1);
		eprefix (prefix);
	}

#ifdef __linux__
	/* Ok, we are ready to go, so setup selinux if applicable */
	setup_selinux (argc, argv);
#endif

	/* Punt the first arg as it's our service name */
	argc--;
	argv++;

	/* Right then, parse any options there may be */
	while ((opt = getopt_long (argc, argv, getoptstring,
							   longopts, (int *) 0)) != -1)
		switch (opt) {
			case 'd':
				setenv ("RC_DEBUG", "yes", 1);
				break;
			case 'h':
				execl (RCSCRIPT_HELP, RCSCRIPT_HELP, service, (char *) NULL);
				eerrorx ("%s: failed to exec `" RCSCRIPT_HELP "': %s",
						 applet, strerror (errno));
			case 's':
				if (! rc_service_state (service, rc_service_started))
					exit (EXIT_FAILURE);
				break;
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
				usage (EXIT_FAILURE);
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

		if (strcmp (optarg, "status") != 0 &&
			strcmp (optarg, "help") != 0) {
			/* Only root should be able to run us */
		}

		/* Export the command we're running.
		   This is important as we stamp on the restart function now but
		   some start/stop routines still need to behave differently if
		   restarting. */
		unsetenv ("RC_CMD");
		setenv ("RC_CMD", optarg, 1);

		doneone = true;
		
		if (strcmp (optarg, "describe") == 0) {
			svc_exec (optarg, NULL);
		} else if (strcmp (optarg, "help") == 0) {
			execl (RCSCRIPT_HELP, RCSCRIPT_HELP, service, "help", (char *) NULL);
			eerrorx ("%s: failed to exec `" RCSCRIPT_HELP "': %s",
					 applet, strerror (errno));
		} else if (strcmp (optarg, "ineed") == 0 ||
				   strcmp (optarg, "iuse") == 0 ||
				   strcmp (optarg, "needsme") == 0 ||
				   strcmp (optarg, "usesme") == 0 ||
				   strcmp (optarg, "iafter") == 0 ||
				   strcmp (optarg, "ibefore") == 0 ||
				   strcmp (optarg, "iprovide") == 0) {
			int depoptions = RC_DEP_TRACE;

			if (rc_is_env ("RC_DEPEND_STRICT", "yes"))
				depoptions |= RC_DEP_STRICT;
			
			if (! deptree && ((deptree = rc_load_deptree ()) == NULL))
				eerrorx ("failed to load deptree");

			rc_strlist_free (types);
			types = rc_strlist_add (NULL, optarg);
			rc_strlist_free (svclist);
			svclist = rc_strlist_add (NULL, applet);
			rc_strlist_free (services);
			services = rc_get_depends (deptree, types, svclist,
									   softlevel, depoptions);
			STRLIST_FOREACH (services, svc, i)
				printf ("%s%s", i == 1 ? "" : " ", svc);
			if (services)
				printf ("\n");
		} else if (strcmp (optarg, "status") == 0) {
			rc_service_state_t r = svc_status (service);
			retval = (int) r;

		} else if (strcmp (optarg, "help") == 0) {
			execl (RCSCRIPT_HELP, RCSCRIPT_HELP, service, "help", (char *) NULL);
			eerrorx ("%s: failed to exec `" RCSCRIPT_HELP "': %s",
					 applet, strerror (errno));
		} else {
			if (geteuid () != 0)
				eerrorx ("%s: root access required", applet);

			if (strcmp (optarg, "conditionalrestart") == 0 ||
				strcmp (optarg, "condrestart") == 0)
			{
				if (rc_service_state (service, rc_service_started))
					svc_restart (deps);
			} else if (strcmp (optarg, "restart") == 0) {
				svc_restart (deps);
			} else if (strcmp (optarg, "start") == 0) {
				svc_start (deps);
			} else if (strcmp (optarg, "stop") == 0) {
				if (deps && in_background)
					get_started_services ();

				svc_stop (deps);

				if (deps) {
					if (! in_background &&
						! rc_runlevel_stopping () &&
						rc_service_state (service, rc_service_stopped))
						uncoldplug ();

					if (in_background &&
						rc_service_state (service, rc_service_inactive))
					{
						int j;
						STRLIST_FOREACH (restart_services, svc, j)
							if (rc_service_state (svc, rc_service_stopped))
								rc_schedule_start_service (service, svc);
					}
				}
			} else if (strcmp (optarg, "zap") == 0) {
				einfo ("Manually resetting %s to stopped state", applet);
				rc_mark_service (applet, rc_service_stopped);
				uncoldplug ();
			} else
				svc_exec (optarg, NULL);

			/* We should ensure this list is empty after an action is done */
			rc_strlist_free (restart_services);
			restart_services = NULL;
		}

		if (! doneone) {
			execl (RCSCRIPT_HELP, RCSCRIPT_HELP, service, (char *) NULL);
			eerrorx ("%s: failed to exec `" RCSCRIPT_HELP "': %s",
					 applet, strerror (errno));
		}
	}

	return (retval);
}
