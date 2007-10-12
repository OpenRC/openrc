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
#include <sys/ioctl.h>
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
#include <termios.h>
#include <unistd.h>

#ifdef __linux__
# include <pty.h>
#else
# include <libutil.h>
#endif

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
static int signal_pipe[2] = { -1, -1 };
static int master_tty = -1;

extern char **environ;

#ifdef __linux__
static void (*selinux_run_init_old) (void);
static void (*selinux_run_init_new) (int argc, char **argv);

static void setup_selinux (int argc, char **argv);

static void setup_selinux (int argc, char **argv)
{
	void *lib_handle = NULL;
	
	if (! exists (SELINUX_LIB))
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

	/* Use new run_init if it exists, else fall back to old */
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
	int serrno = errno;
	char signame[10] = { '\0' };
	struct winsize ws;

	switch (sig) {
		case SIGHUP:
			sighup = true;
			break;

		case SIGCHLD:
			if (signal_pipe[1] > -1) {
				if (write (signal_pipe[1], &sig, sizeof (sig)) == -1)
					eerror ("%s: send: %s", service, strerror (errno));
			} else {
				wait (0);
			}
			break;

		case SIGWINCH:
			if (master_tty >= 0) {
				ioctl (fileno (stdout), TIOCGWINSZ, &ws);
				ioctl (master_tty, TIOCSWINSZ, &ws);
			}
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

	if (! mtime_test || ! exists (mtime_test))
		return (false);

	if (rc_service_state (applet) & RC_SERVICE_STOPPED)
		return (false);

	if (! (mtime = get_mtime (mtime_test, false)))
		return (false);

	while (tests[i]) {
		path = rc_strcatpaths (RC_SVCDIR, tests[i], applet, (char *) NULL);
		if (exists (path)) {
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
	if (exists (cold) && unlink (cold) != 0)
		eerror ("%s: unlink `%s': %s", applet, cold, strerror (errno));
	free (cold);
}

static void start_services (char **list) {
	char *svc;
	int i;
	rc_service_state_t state = rc_service_state (service);

	if (! list)
		return;

	if ((state & RC_SERVICE_INACTIVE ||
		 state & RC_SERVICE_WASINACTIVE) &&
		((state & RC_SERVICE_STARTING) ||
		 (state & RC_SERVICE_STARTED)))
	{
		STRLIST_FOREACH (list, svc, i) {
			if (rc_service_state (svc) & RC_SERVICE_STOPPED) {
				if (state & RC_SERVICE_INACTIVE ||
					state & RC_SERVICE_WASINACTIVE)
				{
					rc_service_schedule_start (service, svc);
					ewarn ("WARNING: %s is scheduled to started when %s has started",
						   svc, applet);
				} else
					rc_service_start (svc);
			}
		}
	}
}

static void restore_state (void)
{
	rc_service_state_t state;

	if (rc_in_plugin || ! in_control ())
		return;

	state = rc_service_state (applet);
	if (state & RC_SERVICE_STOPPING) {
		if (state & RC_SERVICE_WASINACTIVE)
			rc_service_mark (applet, RC_SERVICE_INACTIVE);
		else
			rc_service_mark (applet, RC_SERVICE_STARTED);
		if (rc_runlevel_stopping ())
			rc_service_mark (applet, RC_SERVICE_FAILED);
	} else if (state & RC_SERVICE_STARTING) {
		if (state & RC_SERVICE_WASINACTIVE)
			rc_service_mark (applet, RC_SERVICE_INACTIVE);
		else
			rc_service_mark (applet, RC_SERVICE_STOPPED);
		if (rc_runlevel_starting ())
			rc_service_mark (applet, RC_SERVICE_FAILED);
	}

	if (exclusive)
		unlink (exclusive);
	free (exclusive);
	exclusive = NULL;
}

static void cleanup (void)
{
	restore_state ();
	
	if (! rc_in_plugin) {
		if (prefix_locked)
			unlink (PREFIX_LOCK);
		if (hook_out) {
			rc_plugin_run (hook_out, applet);
			if (hook_out == RC_HOOK_SERVICE_START_DONE)
				rc_plugin_run (RC_HOOK_SERVICE_START_OUT, applet);
			else if (hook_out == RC_HOOK_SERVICE_STOP_DONE)
				rc_plugin_run (RC_HOOK_SERVICE_STOP_OUT, applet);
		}
		
		if (restart_services)
			start_services (restart_services);
	}

	rc_plugin_unload ();
	rc_deptree_free (deptree);
	rc_strlist_free (services);
	rc_strlist_free (types);
	rc_strlist_free (svclist);
	rc_strlist_free (providelist);
	rc_strlist_free (need_services);
	rc_strlist_free (use_services);
	rc_strlist_free (restart_services);
	rc_strlist_free (tmplist);
	free (ibsave);

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
	free (softlevel);
}

static int write_prefix (const char *buffer, size_t bytes, bool *prefixed) {
	unsigned int i;
	const char *ec = ecolor (ECOLOR_HILITE);
	const char *ec_normal = ecolor (ECOLOR_NORMAL);
	ssize_t ret = 0;
	int fd = fileno (stdout);

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

static int wait_pid (pid_t pid)
{
	int status = 0;
	pid_t savedpid = pid;
	int retval = -1;

	errno = 0;
	while ((pid = waitpid (savedpid, &status, 0)) > 0) {
		if (pid == savedpid)
			retval = WIFEXITED (status) ? WEXITSTATUS (status) : EXIT_FAILURE;
	}

	return (retval);
}

static bool svc_exec (const char *arg1, const char *arg2)
{
	bool execok;
	int fdout = fileno (stdout);
	struct termios tt;
	struct winsize ws;
	int i;
	int flags = 0;
	fd_set rset;
	int s;
	char *buffer;
	size_t bytes;
	bool prefixed = false;
	int selfd;
	int slave_tty;

	/* Setup our signal pipe */
	if (pipe (signal_pipe) == -1)
		eerrorx ("%s: pipe: %s", service, applet);
	for (i = 0; i < 2; i++)
		if ((flags = fcntl (signal_pipe[i], F_GETFD, 0) == -1 ||
			 fcntl (signal_pipe[i], F_SETFD, flags | FD_CLOEXEC) == -1))
			eerrorx ("%s: fcntl: %s", service, strerror (errno));

	/* Open a pty for our prefixed output
	 * We do this instead of mapping pipes to stdout, stderr so that
	 * programs can tell if they're attached to a tty or not.
	 * The only loss is that we can no longer tell the difference
	 * between the childs stdout or stderr */
	master_tty = slave_tty = -1;
	if (prefix && isatty (fdout)) {
		tcgetattr (fdout, &tt);
		ioctl (fdout, TIOCGWINSZ, &ws);

		/* If the below call fails due to not enough ptys then we don't
		 * prefix the output, but we still work */
		openpty (&master_tty, &slave_tty, NULL, &tt, &ws);
	}

	service_pid = vfork();
	if (service_pid == -1)
		eerrorx ("%s: vfork: %s", service, strerror (errno));
	if (service_pid == 0) {
		if (slave_tty >= 0) {
			/* Hmmm, this shouldn't work in a vfork, but it does which is
			 * good for us */
			close (master_tty);
	
			dup2 (fileno (stdin), 0);
			dup2 (slave_tty, 1);
			dup2 (slave_tty, 2);
			if (slave_tty > 2)
				close (slave_tty);
		}

		if (exists (RC_SVCDIR "/runscript.sh")) {
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

	selfd = MAX (master_tty, signal_pipe[0]) + 1;
	buffer = xmalloc (sizeof (char) * RC_LINEBUFFER);
	while (1) {
		FD_ZERO (&rset);
		FD_SET (signal_pipe[0], &rset);
		if (master_tty >= 0)
			FD_SET (master_tty, &rset);

		if ((s = select (selfd, &rset, NULL, NULL, NULL)) == -1) {
			if (errno != EINTR) {
				eerror ("%s: select: %s", service, strerror (errno));
				break;
			}
		}
		
		if (s > 0) {
			/* Only SIGCHLD signals come down this pipe */
			if (FD_ISSET (signal_pipe[0], &rset))
				break;

			if (master_tty >= 0 && FD_ISSET (master_tty, &rset)) {
				bytes = read (master_tty, buffer, RC_LINEBUFFER);
				write_prefix (buffer, bytes, &prefixed);
			}
		}
	}
	
	free (buffer);
	close (signal_pipe[0]);
	close (signal_pipe[1]);
	signal_pipe[0] = signal_pipe[1] = -1;

	if (master_tty >= 0) {
		signal (SIGWINCH, SIG_IGN);
		close (master_tty);
		master_tty = -1;
	}

	execok = wait_pid (service_pid) == 0 ? true : false;
	service_pid = 0;

	return (execok);
}

static rc_service_state_t svc_status ()
{
	char status[10];
	int (*e) (const char *fmt, ...) = &einfo;

	rc_service_state_t state = rc_service_state (service);

	if (state & RC_SERVICE_STOPPING) {
		snprintf (status, sizeof (status), "stopping");
		e = &ewarn;
	} else if (state & RC_SERVICE_STOPPING) {
		snprintf (status, sizeof (status), "starting");
		e = &ewarn;
	} else if (state & RC_SERVICE_INACTIVE) {
		snprintf (status, sizeof (status), "inactive");
		e = &ewarn;
	} else if (state & RC_SERVICE_STARTED) {
		if (geteuid () == 0 && rc_service_daemons_crashed (service)) {
			snprintf (status, sizeof (status), "crashed");
			e = &eerror;
		} else
			snprintf (status, sizeof (status), "started");
	} else
		snprintf (status, sizeof (status), "stopped");

	e ("status: %s", status);
	return (state);
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
	mtime_test = xmalloc (sizeof (char) * i);
	snprintf (mtime_test, i, "%s.%d", path, getpid ());
	free (path);

	if (exists (mtime_test) && unlink (mtime_test) != 0) {
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
	tmplist = rc_services_in_state (RC_SERVICE_INACTIVE);

	rc_strlist_free (restart_services);
	restart_services = rc_services_in_state (RC_SERVICE_STARTED);

	STRLIST_FOREACH (tmplist, svc, i)
		rc_strlist_addsort (&restart_services, svc);

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
	rc_service_state_t state;

	hook_out = RC_HOOK_SERVICE_START_OUT;
	rc_plugin_run (RC_HOOK_SERVICE_START_IN, applet);
	state = rc_service_state (service);

	if (rc_env_bool ("IN_HOTPLUG") || in_background) {
		if (! state & RC_SERVICE_INACTIVE &&
			! state & RC_SERVICE_STOPPED)
			exit (EXIT_FAILURE);
		background = true;
	}

	if (state & RC_SERVICE_STARTED)
		ewarnx ("WARNING: %s has already been started", applet);
	else if (state & RC_SERVICE_STOPPING)
		ewarnx ("WARNING: %s is already starting", applet);
	else if (state & RC_SERVICE_STOPPING)
		ewarnx ("WARNING: %s is stopping", applet);
	else if (state & RC_SERVICE_INACTIVE && ! background)
		ewarnx ("WARNING: %s has already started, but is inactive", applet);

	if (! rc_service_mark (service, RC_SERVICE_STOPPING))
		eerrorx ("ERROR: %s has been started by something else", applet);

	make_exclusive (service);

	if (rc_env_bool ("RC_DEPEND_STRICT"))
		depoptions |= RC_DEP_STRICT;

	if (rc_runlevel_starting ())
		depoptions |= RC_DEP_START;

	if (deps) {
		if (! deptree && ((deptree = _rc_deptree_load ()) == NULL))
			eerrorx ("failed to load deptree");

		rc_strlist_free (types);
		types = NULL;
		rc_strlist_add (&types, "broken");
		rc_strlist_free (svclist);
		svclist = NULL;
		rc_strlist_add (&svclist, applet);
		rc_strlist_free (services);
		services = rc_deptree_depends (deptree, types, svclist, softlevel, 0);
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
		types = NULL;
		rc_strlist_add (&types, "ineed");
		rc_strlist_free (need_services);
		need_services = rc_deptree_depends (deptree, types, svclist,
										softlevel, depoptions);

		rc_strlist_add (&types, "iuse");
		rc_strlist_free (use_services);
		use_services = rc_deptree_depends (deptree, types, svclist,
									   softlevel, depoptions);

		if (! rc_runlevel_starting ()) {
			STRLIST_FOREACH (use_services, svc, i)
				if (rc_service_state (svc) & RC_SERVICE_STOPPED) {
					pid_t pid = rc_service_start (svc);
					if (! rc_env_bool ("RC_PARALLEL"))
						wait_pid (pid);
				}
		}

		/* Now wait for them to start */
		rc_strlist_add (&types, "iafter");
		services = rc_deptree_depends (deptree, types, svclist,
								   softlevel, depoptions);

		/* We use tmplist to hold our scheduled by list */
		rc_strlist_free (tmplist);
		tmplist = NULL;

		STRLIST_FOREACH (services, svc, i) {
			rc_service_state_t svcs = rc_service_state (svc);
			if (svcs & RC_SERVICE_STARTED)
				continue;

			/* Don't wait for services which went inactive but are now in
			 * starting state which we are after */
			if (svcs & RC_SERVICE_STOPPING &&
				svcs & RC_SERVICE_WASINACTIVE) {
				bool use = false;
				STRLIST_FOREACH (use_services, svc2, j)
					if (strcmp (svc, svc2) == 0) {
						use = true;
						break;
					}
				if (! use)
					continue;
			}
			
			if (! rc_service_wait (svc))
				eerror ("%s: timed out waiting for %s", applet, svc);
			if ((svcs = rc_service_state (svc)) & RC_SERVICE_STARTED)
				continue;

			STRLIST_FOREACH (need_services, svc2, j)
				if (strcmp (svc, svc2) == 0) {
					if (svcs & RC_SERVICE_INACTIVE ||
						svcs & RC_SERVICE_WASINACTIVE)
						rc_strlist_add (&tmplist, svc);
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
			rc_service_mark (service, RC_SERVICE_STOPPED);
			unlink_mtime_test ();

			rc_strlist_free (types);
			types = NULL;
			rc_strlist_add (&types, "iprovide");
			STRLIST_FOREACH (tmplist, svc, i) {
				rc_service_schedule_start (svc, service);

				rc_strlist_free (svclist);
				svclist = NULL;
				rc_strlist_add (&svclist, svc);
				rc_strlist_free (providelist);
				providelist = rc_deptree_depends (deptree, types, svclist,
											  softlevel, depoptions);
				STRLIST_FOREACH (providelist, svc2, j) 
					rc_service_schedule_start (svc2, service);

				len += strlen (svc) + 2;
				n++;
			}

			len += 5;
			tmp = xmalloc (sizeof (char) * len);
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
	hook_out = RC_HOOK_SERVICE_START_DONE;
	rc_plugin_run (RC_HOOK_SERVICE_START_NOW, applet);
	started = svc_exec ("start", NULL);
	if (ibsave)
		unsetenv ("IN_BACKGROUND");

	if (in_control ()) {
		if (! started)
			eerrorx ("ERROR: %s failed to start", applet);
	} else {
		if (rc_service_state (service) & RC_SERVICE_INACTIVE)
			ewarnx ("WARNING: %s has started, but is inactive", applet);
		else
			ewarnx ("WARNING: %s not under our control, aborting", applet);
	}

	rc_service_mark (service, RC_SERVICE_STARTED);
	unlink_mtime_test ();
	hook_out = RC_HOOK_SERVICE_START_OUT;
	rc_plugin_run (RC_HOOK_SERVICE_START_DONE, applet);

	if (exclusive)
		unlink (exclusive);

	/* Now start any scheduled services */
	rc_strlist_free (services);
	services = rc_services_scheduled (service);
	STRLIST_FOREACH (services, svc, i)
		if (rc_service_state (svc) & RC_SERVICE_STOPPED)
			rc_service_start (svc);
	rc_strlist_free (services);
	services = NULL;

	/* Do the same for any services we provide */
	rc_strlist_free (types);
	types = NULL;
	rc_strlist_add (&types, "iprovide");
	rc_strlist_free (svclist);
	svclist = NULL;
	rc_strlist_add (&svclist, applet);
	rc_strlist_free (tmplist);
	tmplist = rc_deptree_depends (deptree, types, svclist, softlevel, depoptions);

	STRLIST_FOREACH (tmplist, svc2, j) {
		rc_strlist_free (services);
		services = rc_services_scheduled (svc2);
		STRLIST_FOREACH (services, svc, i)
			if (rc_service_state (svc) & RC_SERVICE_STOPPED)
				rc_service_start (svc);
	}

	hook_out = 0;
	rc_plugin_run (RC_HOOK_SERVICE_START_OUT, applet);
}

static void svc_stop (bool deps)
{
	bool stopped;
	rc_service_state_t state = rc_service_state (service);

	hook_out = RC_HOOK_SERVICE_STOP_OUT;
	rc_plugin_run (RC_HOOK_SERVICE_STOP_IN, applet);

	if (rc_runlevel_stopping () &&
		state & RC_SERVICE_FAILED)
		exit (EXIT_FAILURE);

	if (rc_env_bool ("IN_HOTPLUG") || in_background)
		if (! (state & RC_SERVICE_STARTED) && 
			! (state & RC_SERVICE_INACTIVE))
			exit (EXIT_FAILURE);

	if (state & RC_SERVICE_STOPPED)
		ewarnx ("WARNING: %s is already stopped", applet);
	else if (state & RC_SERVICE_STOPPING)
		ewarnx ("WARNING: %s is already stopping", applet);

	if (! rc_service_mark (service, RC_SERVICE_STOPPING))
		eerrorx ("ERROR: %s has been stopped by something else", applet);

	make_exclusive (service);

	if (! rc_runlevel_stopping () &&
		rc_service_in_runlevel (service, RC_LEVEL_BOOT))
		ewarn ("WARNING: you are stopping a boot service");

	if (deps && ! (state & RC_SERVICE_WASINACTIVE)) {
		int depoptions = RC_DEP_TRACE;
		char *svc;
		int i;

		if (rc_env_bool ("RC_DEPEND_STRICT"))
			depoptions |= RC_DEP_STRICT;

		if (rc_runlevel_stopping ())
			depoptions |= RC_DEP_STOP;

		if (! deptree && ((deptree = _rc_deptree_load ()) == NULL))
			eerrorx ("failed to load deptree");

		rc_strlist_free (types);
		types = NULL;
		rc_strlist_add (&types, "needsme");
		rc_strlist_free (svclist);
		svclist = NULL;
		rc_strlist_add (&svclist, applet);
		rc_strlist_free (tmplist);
		tmplist = NULL;
		rc_strlist_free (services);
		services = rc_deptree_depends (deptree, types, svclist,
								   softlevel, depoptions);
		rc_strlist_reverse (services);
		STRLIST_FOREACH (services, svc, i) {
			rc_service_state_t svcs = rc_service_state (svc);
			if (svcs & RC_SERVICE_STARTED || 
				svcs & RC_SERVICE_INACTIVE)
			{
				rc_service_wait (svc);
				svcs = rc_service_state (svc);
				if (svcs & RC_SERVICE_STARTED || 
					svcs & RC_SERVICE_INACTIVE)
				{
					pid_t pid = rc_service_stop (svc);
					if (! rc_env_bool ("RC_PARALLEL"))
						wait_pid (pid);
					rc_strlist_add (&tmplist, svc);
				}
			}
		}
		rc_strlist_free (services);
		services = NULL;

		STRLIST_FOREACH (tmplist, svc, i) {
			if (rc_service_state (svc) & RC_SERVICE_STOPPED)
				continue;

			/* We used to loop 3 times here - maybe re-do this if needed */
			rc_service_wait (svc);
			if (! (rc_service_state (svc) & RC_SERVICE_STOPPED)) {
				if (rc_runlevel_stopping ()) {
					/* If shutting down, we should stop even if a dependant failed */
					if (softlevel &&
						(strcmp (softlevel, RC_LEVEL_SHUTDOWN) == 0 ||
						 strcmp (softlevel, RC_LEVEL_REBOOT) == 0 ||
						 strcmp (softlevel, RC_LEVEL_SINGLE) == 0))
						continue;
					rc_service_mark (service, RC_SERVICE_FAILED);
				}

				eerrorx ("ERROR:  cannot stop %s as %s is still up",
						 applet, svc);
			}
		}
		rc_strlist_free (tmplist);
		tmplist = NULL;

		/* We now wait for other services that may use us and are stopping
		   This is important when a runlevel stops */
		rc_strlist_add (&types, "usesme");
		rc_strlist_add (&types, "ibefore");
		services = rc_deptree_depends (deptree, types, svclist,
								   softlevel, depoptions);
		STRLIST_FOREACH (services, svc, i) {
			if (rc_service_state (svc) & RC_SERVICE_STOPPED)
				continue;
			rc_service_wait (svc);
		}

		rc_strlist_free (services);
		services = NULL;
		rc_strlist_free (types);
		types = NULL;
	}

	if (ibsave)
		setenv ("IN_BACKGROUND", ibsave, 1);
	hook_out = RC_HOOK_SERVICE_STOP_DONE;
	rc_plugin_run (RC_HOOK_SERVICE_STOP_NOW, applet);
	stopped = svc_exec ("stop", NULL);
	if (ibsave)
		unsetenv ("IN_BACKGROUND");

	if (! in_control ())
		ewarnx ("WARNING: %s not under our control, aborting", applet);

	if (! stopped)
		eerrorx ("ERROR: %s failed to stop", applet);

	if (in_background)
		rc_service_mark (service, RC_SERVICE_INACTIVE);
	else
		rc_service_mark (service, RC_SERVICE_STOPPED);

	unlink_mtime_test ();
	hook_out = RC_HOOK_SERVICE_STOP_OUT;
	rc_plugin_run (RC_HOOK_SERVICE_STOP_DONE, applet);
	if (exclusive)
		unlink (exclusive);
	hook_out = 0;
	rc_plugin_run (RC_HOOK_SERVICE_STOP_OUT, applet);

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
		rc_service_state_t state = rc_service_state (service);
		if (state & RC_SERVICE_STARTED || state & RC_SERVICE_INACTIVE)
			svc_exec ("stop", "start");
		else
			svc_exec ("start", NULL);
		return;
	}

	if (! (rc_service_state (service) & RC_SERVICE_STOPPED)) {
		get_started_services ();
		svc_stop (deps);
	}

	svc_start (deps);
	start_services (restart_services);
	rc_strlist_free (restart_services);
	restart_services = NULL;
}

#include "_usage.h"
#define getoptstring "dDsv" getoptstring_COMMON
static struct option longopts[] = {
	{ "debug",      0, NULL, 'd'},
	{ "ifstarted",  0, NULL, 's'},
	{ "nodeps",     0, NULL, 'D'},
	longopts_COMMON
};
static const char * const longopts_help[] = {
	"",
	"",
	"",
	longopts_help_COMMON
};
#undef case_RC_COMMON_getopt_case_h
#define case_RC_COMMON_getopt_case_h \
	execl (RCSCRIPT_HELP, RCSCRIPT_HELP, service, (char *) NULL); \
	eerrorx ("%s: failed to exec `" RCSCRIPT_HELP "': %s", applet, strerror (errno));
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
		service = xstrdup (argv[1]);
	else {
		char pwd[PATH_MAX];
		if (! getcwd (pwd, PATH_MAX))
			eerrorx ("getcwd: %s", strerror (errno));
		service = rc_strcatpaths (pwd, argv[1], (char *) NULL);
	}

	applet = xstrdup (basename (service));
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
	if (exists ("/dev/.rcsysinit")) {
		eerror ("%s: cannot run until sysvinit completes", applet);
		if (mkdir ("/dev/.rcboot", 0755) != 0 && errno != EEXIST)
			eerrorx ("%s: mkdir `/dev/.rcboot': %s", applet, strerror (errno));
		tmp = rc_strcatpaths ("/dev/.rcboot", applet, (char *) NULL);
		symlink (service, tmp);
		exit (EXIT_FAILURE);
	}
#endif

	if ((softlevel = xstrdup (getenv ("RC_SOFTLEVEL"))) == NULL) {
		/* Ensure our environment is pure
		   Also, add our configuration to it */
		tmplist = env_config ();
		env = env_filter ();
		rc_strlist_join (&env, tmplist);
		rc_strlist_free (tmplist);
		tmplist = NULL;

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
				tmp = xstrdup (environ[0]);
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

		softlevel = rc_runlevel_get ();
	}

	setenv ("RC_ELOG", service, 1);
	setenv ("SVCNAME", applet, 1);

	/* Set an env var so that we always know our pid regardless of any
	   subshells the init script may create so that our mark_service_*
	   functions can always instruct us of this change */
	snprintf (pid, sizeof (pid), "%d", (int) getpid ());
	setenv ("RC_RUNSCRIPT_PID", pid, 1);

	/* eprefix is kinda klunky, but it works for our purposes */
	if (rc_env_bool ("RC_PARALLEL")) {
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
		prefix = xmalloc (sizeof (char) * l);
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
			case 's':
				if (! (rc_service_state (service) & RC_SERVICE_STARTED))
					exit (EXIT_FAILURE);
				break;
			case 'D':
				deps = false;
				break;
			case_RC_COMMON_GETOPT
		}

	/* Save the IN_BACKGROUND env flag so it's ONLY passed to the service
	   that is being called and not any dependents */
	if (getenv ("IN_BACKGROUND")) {
		in_background = rc_env_bool ("IN_BACKGROUND");
		ibsave = xstrdup (getenv ("IN_BACKGROUND"));
		unsetenv ("IN_BACKGROUND");
	}

	if (rc_env_bool ("IN_HOTPLUG")) {
		if (! rc_env_bool ("RC_HOTPLUG") || ! rc_service_plugable (applet))
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

			if (rc_env_bool ("RC_DEPEND_STRICT"))
				depoptions |= RC_DEP_STRICT;
			
			if (! deptree && ((deptree = _rc_deptree_load ()) == NULL))
				eerrorx ("failed to load deptree");

			rc_strlist_free (types);
			types = NULL;
			rc_strlist_add (&types, optarg);
			rc_strlist_free (svclist);
			svclist = NULL;
			rc_strlist_add (&svclist, applet);
			rc_strlist_free (services);
			services = rc_deptree_depends (deptree, types, svclist,
									   softlevel, depoptions);
			STRLIST_FOREACH (services, svc, i)
				printf ("%s%s", i == 1 ? "" : " ", svc);
			if (services)
				printf ("\n");
		} else if (strcmp (optarg, "status") == 0) {
			rc_service_state_t r = svc_status (service);
			retval = (int) r;
			if (retval & RC_SERVICE_STARTED)
				retval = 0;
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
				if (rc_service_state (service) & RC_SERVICE_STARTED)
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
						rc_service_state (service) & RC_SERVICE_STOPPED)
						uncoldplug ();

					if (in_background &&
						rc_service_state (service) & RC_SERVICE_INACTIVE)
					{
						int j;
						STRLIST_FOREACH (restart_services, svc, j)
							if (rc_service_state (svc) & RC_SERVICE_STOPPED)
								rc_service_schedule_start (service, svc);
					}
				}
			} else if (strcmp (optarg, "zap") == 0) {
				einfo ("Manually resetting %s to stopped state", applet);
				rc_service_mark (applet, RC_SERVICE_STOPPED);
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
