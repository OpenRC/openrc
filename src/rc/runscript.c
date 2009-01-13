/*
 * runscript.c
 * Handle launching of init scripts.
 */

/*
 * Copyright 2007-2009 Roy Marples <roy@marples.name>
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

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/file.h>
#include <sys/param.h>
#include <sys/stat.h>

#include <ctype.h>
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <fnmatch.h>
#include <getopt.h>
#include <libgen.h>
#include <limits.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#ifdef __linux__
# include <pty.h>
#elif defined(__NetBSD__) || defined(__OpenBSD__)
# include <util.h>
#else
# include <libutil.h>
#endif

#include "builtins.h"
#include "einfo.h"
#include "rc.h"
#include "rc-misc.h"
#include "rc-plugin.h"

#define SELINUX_LIB     RC_LIBDIR "/runscript_selinux.so"

#define PREFIX_LOCK	RC_SVCDIR "/prefix.lock"

/* usecs to wait while we poll the fifo */
#define WAIT_INTERVAL	20000000

/* max secs to wait until a service comes up */
#define WAIT_MAX        300

#define ONE_SECOND      1000000000

static const char *applet = NULL;
static RC_STRINGLIST *applet_list = NULL;
static RC_STRINGLIST *restart_services = NULL;
static RC_STRINGLIST *need_services = NULL;
static RC_STRINGLIST *use_services = NULL;
static RC_STRINGLIST *services = NULL;
static RC_STRINGLIST *tmplist = NULL;
static char *service = NULL;
static char exclusive[PATH_MAX] = { '\0' };
static char mtime_test[PATH_MAX] = { '\0' };
static RC_DEPTREE *deptree = NULL;
static char *runlevel = NULL;
static bool sighup = false;
static char *ibsave = NULL;
static bool in_background = false;
static RC_HOOK hook_out = 0;
static pid_t service_pid = 0;
static char *prefix = NULL;
static int signal_pipe[2] = { -1, -1 };
static int master_tty = -1;

static RC_STRINGLIST *types_b = NULL;
static RC_STRINGLIST *types_n = NULL;
static RC_STRINGLIST *types_nu = NULL;
static RC_STRINGLIST *types_nua = NULL;
static RC_STRINGLIST *types_m = NULL;
static RC_STRINGLIST *types_mua = NULL;

#ifdef __linux__
static void (*selinux_run_init_old)(void);
static void (*selinux_run_init_new)(int argc, char **argv);

static void
setup_selinux(int argc, char **argv)
{
	void *lib_handle = NULL;

	if (! exists(SELINUX_LIB))
		return;

	lib_handle = dlopen(SELINUX_LIB, RTLD_NOW | RTLD_GLOBAL);
	if (! lib_handle) {
		eerror("dlopen: %s", dlerror());
		return;
	}

	selinux_run_init_old = (void (*)(void))
		dlfunc(lib_handle, "selinux_runscript");
	selinux_run_init_new = (void (*)(int, char **))
		dlfunc(lib_handle, "selinux_runscript2");

	/* Use new run_init if it exists, else fall back to old */
	if (selinux_run_init_new)
		selinux_run_init_new(argc, argv);
	else if (selinux_run_init_old)
		selinux_run_init_old();
	else
		/* This shouldnt happen... probably corrupt lib */
		eerrorx("run_init is missing from runscript_selinux.so!");

	dlclose(lib_handle);
}
#endif

static void
handle_signal(int sig)
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
			if (write(signal_pipe[1], &sig, sizeof(sig)) == -1)
				eerror("%s: send: %s",
				       service, strerror(errno));
		} else
			rc_waitpid(-1);
		break;

	case SIGWINCH:
		if (master_tty >= 0) {
			ioctl(fileno(stdout), TIOCGWINSZ, &ws);
			ioctl(master_tty, TIOCSWINSZ, &ws);
		}
		break;

	case SIGINT:
		if (!signame[0])
			snprintf(signame, sizeof(signame), "SIGINT");
		/* FALLTHROUGH */
	case SIGTERM:
		if (!signame[0])
			snprintf(signame, sizeof(signame), "SIGTERM");
		/* FALLTHROUGH */
	case SIGQUIT:
		if (!signame[0])
			snprintf(signame, sizeof(signame), "SIGQUIT");
		/* Send the signal to our children too */
		if (service_pid > 0)
			kill(service_pid, sig);
		eerrorx("%s: caught %s, aborting", applet, signame);
		/* NOTREACHED */

	default:
		eerror("%s: caught unknown signal %d", applet, sig);
	}

	/* Restore errno */
	errno = serrno;
}

static time_t
get_mtime(const char *pathname, bool follow_link)
{
	struct stat buf;
	int retval;

	if (!pathname)
		return 0;
	retval = follow_link ? stat(pathname, &buf) : lstat(pathname, &buf);
	if (!retval)
		return buf.st_mtime;
	errno = 0;
	return 0;
}

static const char *const tests[] = {
	"starting", "started", "stopping", "inactive", "wasinactive", NULL
};
static bool
in_control()
{
	char file[PATH_MAX];
	time_t m;
	time_t mtime;
	int i = 0;

	if (sighup)
		return false;

	if (!*mtime_test || !exists(mtime_test))
		return false;

	if (rc_service_state(applet) & RC_SERVICE_STOPPED)
		return false;

	if (!(mtime = get_mtime(mtime_test, false)))
		return false;

	while (tests[i]) {
		snprintf(file, sizeof(file), RC_SVCDIR "/%s/%s",
			 tests[i], applet);
		if (exists(file)) {
			m = get_mtime(file, false);
			if (mtime < m && m != 0)
				return false;
		}
		i++;
	}

	return true;
}

static void
unhotplug()
{
	char file[PATH_MAX];

	snprintf(file, sizeof(file), RC_SVCDIR "/hotplugged/%s", applet);
	if (exists(file) && unlink(file) != 0)
		eerror("%s: unlink `%s': %s", applet, file, strerror(errno));
}

static void
start_services(RC_STRINGLIST *list)
{
	RC_STRING *svc;
	RC_SERVICE state = rc_service_state (service);

	if (!list)
		return;

	if (state & RC_SERVICE_INACTIVE ||
	    state & RC_SERVICE_WASINACTIVE ||
	    state & RC_SERVICE_STARTING ||
	    state & RC_SERVICE_STARTED)
	{
		TAILQ_FOREACH(svc, list, entries) {
			if (!(rc_service_state(svc->value) &
			      RC_SERVICE_STOPPED))
				continue;
			if (state & RC_SERVICE_INACTIVE ||
			    state & RC_SERVICE_WASINACTIVE)
			{
				rc_service_schedule_start(service,
							  svc->value);
				ewarn("WARNING: %s is scheduled to started"
				      " when %s has started",
				       svc->value, applet);
			} else
				service_start(svc->value);
		}
	}
}

static void
restore_state(void)
{
	RC_SERVICE state;

	if (rc_in_plugin || !in_control())
		return;
	state = rc_service_state(applet);
	if (state & RC_SERVICE_STOPPING) {
		if (state & RC_SERVICE_WASINACTIVE)
			rc_service_mark(applet, RC_SERVICE_INACTIVE);
		else
			rc_service_mark(applet, RC_SERVICE_STARTED);
		if (rc_runlevel_stopping())
			rc_service_mark(applet, RC_SERVICE_FAILED);
	} else if (state & RC_SERVICE_STARTING) {
		if (state & RC_SERVICE_WASINACTIVE)
			rc_service_mark(applet, RC_SERVICE_INACTIVE);
		else
			rc_service_mark(applet, RC_SERVICE_STOPPED);
		if (rc_runlevel_starting())
			rc_service_mark(applet, RC_SERVICE_FAILED);
	}

	if (*exclusive) {
		unlink(exclusive);
		*exclusive = '\0';
	}
}

static void
cleanup(void)
{
	restore_state();

	if (!rc_in_plugin) {
		if (hook_out) {
			rc_plugin_run(hook_out, applet);
			if (hook_out == RC_HOOK_SERVICE_START_DONE)
				rc_plugin_run(RC_HOOK_SERVICE_START_OUT,
					      applet);
			else if (hook_out == RC_HOOK_SERVICE_STOP_DONE)
				rc_plugin_run(RC_HOOK_SERVICE_STOP_OUT,
					      applet);
		}

		if (restart_services)
			start_services(restart_services);
	}

	rc_plugin_unload();

#ifdef DEBUG_MEMORY
	rc_stringlist_free(types_b);
	rc_stringlist_free(types_n);
	rc_stringlist_free(types_nu);
	rc_stringlist_free(types_nua);
	rc_stringlist_free(types_m);
	rc_stringlist_free(types_mua);
	rc_deptree_free(deptree);
	rc_stringlist_free(restart_services);
	rc_stringlist_free(need_services);
	rc_stringlist_free(use_services);
	rc_stringlist_free(services);
	rc_stringlist_free(applet_list);
	rc_stringlist_free(tmplist);
	free(ibsave);
	free(service);
	free(prefix);
	free(runlevel);
#endif

	if (*mtime_test && !rc_in_plugin)
		unlink(mtime_test);
}

static int
write_prefix(const char *buffer, size_t bytes, bool *prefixed)
{
	size_t i, j;
	const char *ec = ecolor(ECOLOR_HILITE);
	const char *ec_normal = ecolor(ECOLOR_NORMAL);
	ssize_t ret = 0;
	int fd = fileno(stdout), lock_fd = -1;

	/* Spin until we lock the prefix */
	for (;;) {
		lock_fd = open(PREFIX_LOCK, O_WRONLY | O_CREAT, 0664);
		if (lock_fd != -1)
			if (flock(lock_fd, LOCK_EX) == 0)
				break;
		close(lock_fd);
	}

	for (i = 0; i < bytes; i++) {
		/* We don't prefix eend calls (cursor up) */
		if (buffer[i] == '\033' && !*prefixed) {
			for (j = i + 1; j < bytes; j++) {
				if (buffer[j] == 'A')
					*prefixed = true;
				if (isalpha((unsigned int)buffer[j]))
					break;
			}
		}

		if (!*prefixed) {
			ret += write(fd, ec, strlen(ec));
			ret += write(fd, prefix, strlen(prefix));
			ret += write(fd, ec_normal, strlen(ec_normal));
			ret += write(fd, "|", 1);
			*prefixed = true;
		}

		if (buffer[i] == '\n')
			*prefixed = false;
		ret += write(fd, buffer + i, 1);
	}

	/* Release the lock */
	close(lock_fd);
	return ret;
}

static bool
svc_exec(const char *arg1, const char *arg2)
{
	bool execok;
	int fdout = fileno(stdout);
	struct termios tt;
	struct winsize ws;
	int i;
	int flags = 0;
	struct pollfd fd[2];
	int s;
	char *buffer;
	size_t bytes;
	bool prefixed = false;
	int slave_tty;

	/* Setup our signal pipe */
	if (pipe(signal_pipe) == -1)
		eerrorx("%s: pipe: %s", service, applet);
	for (i = 0; i < 2; i++)
		if ((flags = fcntl(signal_pipe[i], F_GETFD, 0) == -1 ||
		     fcntl(signal_pipe[i], F_SETFD, flags | FD_CLOEXEC) == -1))
			eerrorx("%s: fcntl: %s", service, strerror(errno));

	/* Open a pty for our prefixed output
	 * We do this instead of mapping pipes to stdout, stderr so that
	 * programs can tell if they're attached to a tty or not.
	 * The only loss is that we can no longer tell the difference
	 * between the childs stdout or stderr */
	master_tty = slave_tty = -1;
	if (prefix && isatty(fdout)) {
		tcgetattr(fdout, &tt);
		ioctl(fdout, TIOCGWINSZ, &ws);

		/* If the below call fails due to not enough ptys then we don't
		 * prefix the output, but we still work */
		openpty(&master_tty, &slave_tty, NULL, &tt, &ws);
		if (master_tty >= 0 &&
		    (flags = fcntl(master_tty, F_GETFD, 0)) == 0)
		     fcntl(master_tty, F_SETFD, flags | FD_CLOEXEC);

		if (slave_tty >=0 &&
		    (flags = fcntl(slave_tty, F_GETFD, 0)) == 0)
		     fcntl(slave_tty, F_SETFD, flags | FD_CLOEXEC);
	}

	service_pid = fork();
	if (service_pid == -1)
		eerrorx("%s: fork: %s", service, strerror(errno));
	if (service_pid == 0) {
		if (slave_tty >= 0) {
			dup2(slave_tty, STDOUT_FILENO);
			dup2(slave_tty, STDERR_FILENO);
		}

		if (exists(RC_SVCDIR "/runscript.sh")) {
			execl(RC_SVCDIR "/runscript.sh",
			      RC_SVCDIR "/runscript.sh",
			      service, arg1, arg2, (char *) NULL);
			eerror("%s: exec `" RC_SVCDIR "/runscript.sh': %s",
				service, strerror(errno));
			_exit(EXIT_FAILURE);
		} else {
			execl(RC_LIBDIR "/sh/runscript.sh",
			      RC_LIBDIR "/sh/runscript.sh",
			      service, arg1, arg2, (char *) NULL);
			eerror("%s: exec `" RC_LIBDIR "/sh/runscript.sh': %s",
			       service, strerror(errno));
			_exit(EXIT_FAILURE);
		}
	}

	buffer = xmalloc(sizeof(char) * BUFSIZ);
	fd[0].fd = signal_pipe[0];
	fd[0].events = fd[1].events = POLLIN;
	fd[0].revents = fd[1].revents = 0;
	if (master_tty >= 0) {
		fd[1].fd = master_tty;
		fd[1].events = POLLIN;
		fd[1].revents = 0;
	}

	for (;;) {
		if ((s = poll(fd, master_tty >= 0 ? 2 : 1, -1)) == -1) {
			if (errno != EINTR) {
				eerror("%s: poll: %s",
					service, strerror(errno));
				break;
			}
		}

		if (s > 0) {
			if (fd[1].revents & (POLLIN | POLLHUP)) {
				bytes = read(master_tty, buffer, BUFSIZ);
				write_prefix(buffer, bytes, &prefixed);
			}

			/* Only SIGCHLD signals come down this pipe */
			if (fd[0].revents & (POLLIN | POLLHUP))
				break;
		}
	}

	free(buffer);
	close(signal_pipe[0]);
	close(signal_pipe[1]);
	signal_pipe[0] = signal_pipe[1] = -1;

	if (master_tty >= 0) {
		/* Why did we do this? */
		/* signal (SIGWINCH, SIG_IGN); */
		close(master_tty);
		master_tty = -1;
	}

	execok = rc_waitpid(service_pid) == 0 ? true : false;
	if (!execok && errno == ECHILD)
		/* killall5 -9 could cause this */
		execok = true;
	service_pid = 0;

	return execok;
}

static bool
svc_wait(const char *svc)
{
	char fifo[PATH_MAX];
	struct timespec ts;
	int nloops = WAIT_MAX * (ONE_SECOND / WAIT_INTERVAL);
	int sloops = (ONE_SECOND / WAIT_INTERVAL) * 5;
	bool retval = false;
	bool forever = false;
	RC_STRINGLIST *keywords;

	/* Some services don't have a timeout, like fsck */
	keywords = rc_deptree_depend(deptree, svc, "keyword");
	if (rc_stringlist_find(keywords, "notimeout"))
		forever = true;
	rc_stringlist_free(keywords);

	snprintf(fifo, sizeof(fifo), RC_SVCDIR "/exclusive/%s",
		 basename_c(svc));
	ts.tv_sec = 0;
	ts.tv_nsec = WAIT_INTERVAL;

	while (nloops) {
		if (!exists(fifo)) {
			retval = true;
			break;
		}

		if (nanosleep(&ts, NULL) == -1) {
			if (errno != EINTR)
				break;
		}

		if (!forever) {
			nloops --;

			if (--sloops == 0) {
				ewarn("%s: waiting for %s", applet, svc);
				sloops = (ONE_SECOND / WAIT_INTERVAL) * 5;
			}
		}
	}

	if (!exists(fifo))
		retval = true;
	return retval;
}

static RC_SERVICE
svc_status(void)
{
	char status[10];
	int (*e) (const char *fmt, ...) EINFO_PRINTF(1, 2) = einfo;
	RC_SERVICE state = rc_service_state(service);

	if (state & RC_SERVICE_STOPPING) {
		snprintf(status, sizeof(status), "stopping");
		e = ewarn;
	} else if (state & RC_SERVICE_STARTING) {
		snprintf(status, sizeof(status), "starting");
		e = ewarn;
	} else if (state & RC_SERVICE_INACTIVE) {
		snprintf(status, sizeof(status), "inactive");
		e = ewarn;
	} else if (state & RC_SERVICE_STARTED) {
		errno = 0;
		if (_rc_can_find_pids() &&
		    rc_service_daemons_crashed(service) &&
		    errno != EACCES)
		{
			snprintf(status, sizeof(status), "crashed");
			e = eerror;
		} else
			snprintf(status, sizeof(status), "started");
	} else
		snprintf(status, sizeof(status), "stopped");

	e("status: %s", status);
	return state;
}

static void
make_exclusive(void)
{
	/* We create a fifo so that other services can wait until we complete */
	if (!*exclusive)
		snprintf(exclusive, sizeof(exclusive),
			 RC_SVCDIR "/exclusive/%s", applet);

	if (mkfifo(exclusive, 0600) != 0 && errno != EEXIST &&
	    (errno != EACCES || geteuid () == 0))
		eerrorx ("%s: unable to create fifo `%s': %s",
			 applet, exclusive, strerror(errno));

	snprintf(mtime_test, sizeof(mtime_test),
		 RC_SVCDIR "/exclusive/%s.%d", applet, getpid());

	if (exists(mtime_test) && unlink(mtime_test) != 0) {
		eerror("%s: unlink `%s': %s",
		       applet, mtime_test, strerror(errno));
		*mtime_test = '\0';
		return;
	}

	if (symlink(service, mtime_test) != 0) {
		eerror("%s: symlink `%s' to `%s': %s",
		       applet, service, mtime_test, strerror(errno));
		*mtime_test = '\0';
	}
}

static void
unlink_mtime_test(void)
{
	if (unlink(mtime_test) != 0)
		eerror("%s: unlink `%s': %s",
		       applet, mtime_test, strerror(errno));
	*mtime_test = '\0';
}

static void
get_started_services(void)
{
	RC_STRINGLIST *tmp = rc_services_in_state(RC_SERVICE_INACTIVE);

	rc_stringlist_free(restart_services);
	restart_services = rc_services_in_state(RC_SERVICE_STARTED);
	TAILQ_CONCAT(restart_services, tmp, entries);
	free(tmp);
}

static void
setup_types(void)
{
	types_b = rc_stringlist_new();
	rc_stringlist_add(types_b, "broken");

	types_n = rc_stringlist_new();
	rc_stringlist_add(types_n, "ineed");

	types_nu = rc_stringlist_new();
	rc_stringlist_add(types_nu, "ineed");
	rc_stringlist_add(types_nu, "iuse");

	types_nua = rc_stringlist_new();
	rc_stringlist_add(types_nua, "ineed");
	rc_stringlist_add(types_nua, "iuse");
	rc_stringlist_add(types_nua, "iafter");

	types_m = rc_stringlist_new();
	rc_stringlist_add(types_m, "needsme");

	types_mua = rc_stringlist_new();
	rc_stringlist_add(types_mua, "needsme");
	rc_stringlist_add(types_mua, "usesme");
	rc_stringlist_add(types_mua, "beforeme");
}

static void
svc_start(bool deps)
{
	bool started;
	bool background = false;
	RC_STRING *svc;
	RC_STRING *svc2;
	int depoptions = RC_DEP_TRACE;
	RC_SERVICE state;
	bool first;
	int n;
	size_t len;
	char *p;
	char *tmp;

	state = rc_service_state(service);

	if (rc_yesno(getenv("IN_HOTPLUG")) || in_background) {
		if (! state & RC_SERVICE_INACTIVE &&
		    ! state & RC_SERVICE_STOPPED)
			exit(EXIT_FAILURE);
		background = true;
		rc_service_mark(service, RC_SERVICE_HOTPLUGGED);
		if (strcmp(runlevel, RC_LEVEL_SYSINIT) == 0)
			ewarnx("WARNING: %s will be started in the"
			       " next runlevel", applet);
	}

	if (state & RC_SERVICE_STARTED) {
		ewarn("WARNING: %s has already been started", applet);
		return;
	} else if (state & RC_SERVICE_STARTING)
		ewarnx("WARNING: %s is already starting", applet);
	else if (state & RC_SERVICE_STOPPING)
		ewarnx("WARNING: %s is stopping", applet);
	else if (state & RC_SERVICE_INACTIVE && ! background)
		ewarnx("WARNING: %s has already started, but is inactive",
		       applet);

	if (!rc_service_mark(service, RC_SERVICE_STARTING)) {
		if (errno == EACCES)
			eerrorx("%s: superuser access required", applet);
		eerrorx("ERROR: %s has been started by something else", applet);
	}

	make_exclusive();
	hook_out = RC_HOOK_SERVICE_START_OUT;
	rc_plugin_run(RC_HOOK_SERVICE_START_IN, applet);

	errno = 0;
	if (rc_conf_yesno("rc_depend_strict") || errno == ENOENT)
		depoptions |= RC_DEP_STRICT;

	if (deps) {
		if (!deptree && ((deptree = _rc_deptree_load(0, NULL)) == NULL))
			eerrorx("failed to load deptree");
		if (!types_b)
			setup_types();

		services = rc_deptree_depends(deptree, types_b, applet_list,
				runlevel, 0);
		if (TAILQ_FIRST(services)) {
			eerrorn("ERROR: `%s' needs ", applet);
			first = true;
			TAILQ_FOREACH(svc, services, entries) {
				if (first)
					first = false;
				else
					fprintf(stderr, ", ");
				fprintf(stderr, "%s", svc->value);
			}
			exit(EXIT_FAILURE);
		}
		rc_stringlist_free(services);
		services = NULL;

		need_services = rc_deptree_depends(deptree, types_n,
						   applet_list,	runlevel,
						   depoptions);
		use_services = rc_deptree_depends(deptree, types_nu,
						  applet_list, runlevel,
						  depoptions);

		if (!rc_runlevel_starting()) {
			TAILQ_FOREACH(svc, use_services, entries) {
				state = rc_service_state(svc->value);
				/* Don't stop failed services again.
				 * If you remove this check, ensure that the
				 * exclusive file isn't created. */
				if (state & RC_SERVICE_FAILED &&
				    rc_runlevel_starting())
					continue;
				if (state & RC_SERVICE_STOPPED) {
					pid_t pid = service_start(svc->value);
					if (! rc_conf_yesno("rc_parallel"))
						rc_waitpid(pid);
				}
			}
		}

		/* Now wait for them to start */
		services = rc_deptree_depends(deptree, types_nua, applet_list,
					      runlevel, depoptions);
		/* We use tmplist to hold our scheduled by list */
		tmplist = rc_stringlist_new();
		TAILQ_FOREACH(svc, services, entries) {
			state = rc_service_state(svc->value);
			if (state & RC_SERVICE_STARTED)
				continue;

			/* Don't wait for services which went inactive but are
			 * now in starting state which we are after */
			if (state & RC_SERVICE_STARTING &&
			    state & RC_SERVICE_WASINACTIVE)
			{
				if (!rc_stringlist_find(need_services,
							svc->value) &&
				    !rc_stringlist_find(use_services,
							svc->value))
					continue;
			}

			if (!svc_wait(svc->value))
				eerror("%s: timed out waiting for %s",
				       applet, svc->value);
			state = rc_service_state(svc->value);
			if (state & RC_SERVICE_STARTED)
				continue;
			if (rc_stringlist_find(need_services, svc->value)) {
				if (state & RC_SERVICE_INACTIVE ||
				    state & RC_SERVICE_WASINACTIVE)
				{
					rc_stringlist_add(tmplist, svc->value);
				} else if (!TAILQ_FIRST(tmplist))
					eerrorx("ERROR: cannot start %s as"
						" %s would not start",
						applet, svc->value);
			}
		}

		if (TAILQ_FIRST(tmplist)) {
			/* Set the state now, then unlink our exclusive so that
			   our scheduled list is preserved */
			rc_service_mark(service, RC_SERVICE_STOPPED);
			unlink_mtime_test();

			rc_stringlist_free(use_services);
			use_services = NULL;
			len = 0;
			n = 0;
			TAILQ_FOREACH(svc, tmplist, entries) {
				rc_service_schedule_start(svc->value, service);
				use_services = rc_deptree_depend(deptree, 
								 "iprovide",
								 svc->value);
				TAILQ_FOREACH(svc2, use_services, entries)
					rc_service_schedule_start(svc2->value,
								  service);
				rc_stringlist_free(use_services);
				use_services = NULL;
				len += strlen(svc->value) + 2;
				n++;
			}

			len += 5;
			tmp = p = xmalloc(sizeof(char) * len);
			TAILQ_FOREACH(svc, tmplist, entries) {
				if (p != tmp)
					p += snprintf(p, len, ", ");
				p += snprintf(p, len - (p - tmp),
					      "%s", svc->value);
			}
			rc_stringlist_free(tmplist);
			tmplist = NULL;
			ewarnx("WARNING: %s is scheduled to start when "
			       "%s has started", applet, tmp);
			free(tmp);
		}

		rc_stringlist_free(services);
		services = NULL;
	}

	if (ibsave)
		setenv("IN_BACKGROUND", ibsave, 1);
	hook_out = RC_HOOK_SERVICE_START_DONE;
	rc_plugin_run(RC_HOOK_SERVICE_START_NOW, applet);
	started = svc_exec("start", NULL);
	if (ibsave)
		unsetenv("IN_BACKGROUND");

	if (in_control()) {
		if (!started)
			eerrorx("ERROR: %s failed to start", applet);
	} else {
		if (rc_service_state(service) & RC_SERVICE_INACTIVE)
			ewarnx("WARNING: %s has started, but is inactive",
			       applet);
		else
			ewarnx("WARNING: %s not under our control, aborting",
			       applet);
	}

	rc_service_mark(service, RC_SERVICE_STARTED);
	unlink_mtime_test();
	hook_out = RC_HOOK_SERVICE_START_OUT;
	rc_plugin_run(RC_HOOK_SERVICE_START_DONE, applet);
	unlink(exclusive);

	/* Now start any scheduled services */
	services = rc_services_scheduled(service);
	TAILQ_FOREACH(svc, services, entries)
		if (rc_service_state(svc->value) & RC_SERVICE_STOPPED)
			service_start(svc->value);
	rc_stringlist_free(services);
	services = NULL;

	/* Do the same for any services we provide */
	if (deptree) {
		tmplist = rc_deptree_depend(deptree, "iprovide", applet);
		TAILQ_FOREACH(svc, tmplist, entries) {
			services = rc_services_scheduled(svc->value);
			TAILQ_FOREACH(svc2, services, entries)
				if (rc_service_state(svc2->value) &
				    RC_SERVICE_STOPPED)
					service_start(svc2->value);
			rc_stringlist_free(services);
			services = NULL;
		}
		rc_stringlist_free(tmplist);
		tmplist = NULL;
	}

	hook_out = 0;
	rc_plugin_run(RC_HOOK_SERVICE_START_OUT, applet);
}

static void
svc_stop(bool deps)
{
	bool stopped;
	RC_SERVICE state = rc_service_state(service);
	int depoptions = RC_DEP_TRACE;
	RC_STRING *svc;


	if (rc_runlevel_stopping() && state & RC_SERVICE_FAILED)
		exit(EXIT_FAILURE);

	if (rc_yesno(getenv("IN_HOTPLUG")) || in_background)
		if (!(state & RC_SERVICE_STARTED) &&
		    !(state & RC_SERVICE_INACTIVE))
			exit (EXIT_FAILURE);

	if (state & RC_SERVICE_STOPPED) {
		ewarn("WARNING: %s is already stopped", applet);
		return;
	} else if (state & RC_SERVICE_STOPPING)
		ewarnx("WARNING: %s is already stopping", applet);

	if (!rc_service_mark(service, RC_SERVICE_STOPPING)) {
		if (errno == EACCES)
			eerrorx("%s: superuser access required", applet);
		eerrorx("ERROR: %s has been stopped by something else", applet);
	}

	make_exclusive();

	hook_out = RC_HOOK_SERVICE_STOP_OUT;
	rc_plugin_run(RC_HOOK_SERVICE_STOP_IN, applet);

	if (!rc_runlevel_stopping()) {
		if (rc_service_in_runlevel(service, RC_LEVEL_SYSINIT))
			ewarn ("WARNING: you are stopping a sysinit service");
		else if (rc_service_in_runlevel(service, RC_LEVEL_BOOT))
			ewarn ("WARNING: you are stopping a boot service");
	}

	if (deps && !(state & RC_SERVICE_WASINACTIVE)) {
		errno = 0;
		if (rc_conf_yesno("rc_depend_strict") || errno == ENOENT)
			depoptions |= RC_DEP_STRICT;

		if (!deptree && ((deptree = _rc_deptree_load(0, NULL)) == NULL))
			eerrorx("failed to load deptree");

		if (!types_m)
			setup_types();

		services = rc_deptree_depends(deptree, types_m, applet_list,
					      runlevel, depoptions);
		tmplist = rc_stringlist_new();
		TAILQ_FOREACH_REVERSE(svc, services, rc_stringlist, entries) {
			state = rc_service_state(svc->value);
			/* Don't stop failed services again.
			 * If you remove this check, ensure that the
			 * exclusive file isn't created. */
			if (state & RC_SERVICE_FAILED &&
			    rc_runlevel_stopping())
				continue;
			if (state & RC_SERVICE_STARTED ||
			    state & RC_SERVICE_INACTIVE)
			{
				svc_wait(svc->value);
				state = rc_service_state(svc->value);
				if (state & RC_SERVICE_STARTED ||
				    state & RC_SERVICE_INACTIVE)
				{
					pid_t pid = service_stop(svc->value);
					if (!rc_conf_yesno("rc_parallel"))
						rc_waitpid(pid);
					rc_stringlist_add(tmplist, svc->value);
				}
			}
		}
		rc_stringlist_free(services);
		services = NULL;

		TAILQ_FOREACH(svc, tmplist, entries) {
			if (rc_service_state(svc->value) & RC_SERVICE_STOPPED)
				continue;
			svc_wait(svc->value);
			if (rc_service_state(svc->value) & RC_SERVICE_STOPPED)
				continue;
			if (rc_runlevel_stopping()) {
				/* If shutting down, we should stop even
				 * if a dependant failed */
				if (runlevel &&
				    (strcmp(runlevel,
					    RC_LEVEL_SHUTDOWN) == 0 ||
				     strcmp(runlevel,
					    RC_LEVEL_SINGLE) == 0))
					continue;
				rc_service_mark(service, RC_SERVICE_FAILED);
			}
			eerrorx("ERROR: cannot stop %s as %s "
				"is still up", applet, svc->value);
		}
		rc_stringlist_free(tmplist);
		tmplist = NULL;
	

		/* We now wait for other services that may use us and are
		 * stopping. This is important when a runlevel stops */
		services = rc_deptree_depends(deptree, types_mua, applet_list,
					      runlevel, depoptions);
		TAILQ_FOREACH(svc, services, entries) {
			if (rc_service_state(svc->value) & RC_SERVICE_STOPPED)
				continue;
			svc_wait(svc->value);
		}
		rc_stringlist_free(services);
		services = NULL;
	}

	/* If we're stopping localmount, set LC_ALL=C so that
	 * bash doesn't load anything blocking the unmounting of /usr */
	if (strcmp(applet, "localmount") == 0)
		setenv("LC_ALL", "C", 1);

	if (ibsave)
		setenv("IN_BACKGROUND", ibsave, 1);
	hook_out = RC_HOOK_SERVICE_STOP_DONE;
	rc_plugin_run(RC_HOOK_SERVICE_STOP_NOW, applet);
	stopped = svc_exec("stop", NULL);
	if (ibsave)
		unsetenv("IN_BACKGROUND");

	if (!in_control())
		ewarnx("WARNING: %s not under our control, aborting", applet);

	if (!stopped)
		eerrorx("ERROR: %s failed to stop", applet);

	if (in_background)
		rc_service_mark(service, RC_SERVICE_INACTIVE);
	else
		rc_service_mark(service, RC_SERVICE_STOPPED);

	unlink_mtime_test();
	hook_out = RC_HOOK_SERVICE_STOP_OUT;
	rc_plugin_run(RC_HOOK_SERVICE_STOP_DONE, applet);
	unlink(exclusive);
	hook_out = 0;
	rc_plugin_run(RC_HOOK_SERVICE_STOP_OUT, applet);
}

static void
svc_restart(bool deps)
{
	/* This is hairly and a better way needs to be found I think!
	 * The issue is this - openvpn need net and dns. net can restart
	 * dns via resolvconf, so you could have openvpn trying to restart
	 * dnsmasq which in turn is waiting on net which in turn is waiting
	 * on dnsmasq.
	 * The work around is for resolvconf to restart it's services with
	 * --nodeps which means just that.
	 * The downside is that there is a small window when our status is
	 * invalid.
	 * One workaround would be to introduce a new status,
	 * or status locking. */
	if (!deps) {
		RC_SERVICE state = rc_service_state(service);
		if (state & RC_SERVICE_STARTED || state & RC_SERVICE_INACTIVE)
			svc_exec("stop", "start");
		else
			svc_exec("start", NULL);
		return;
	}

	if (!(rc_service_state(service) & RC_SERVICE_STOPPED)) {
		get_started_services();
		svc_stop(deps);
	}

	svc_start(deps);
	start_services(restart_services);
	rc_stringlist_free(restart_services);
	restart_services = NULL;
}

static bool
service_plugable(void)
{
	char *list, *p, *token;
	bool allow = true, truefalse;
	char *match = rc_conf_value("rc_hotplug");

	if (!match)
		match = rc_conf_value("rc_plug_services");
	if (!match)
		return false;

	list = xstrdup(match);
	p = list;
	while ((token = strsep(&p, " "))) {
		if (token[0] == '!') {
			truefalse = false;
			token++;
		} else
			truefalse = true;

		if (fnmatch(token, applet, 0) == 0) {
			allow = truefalse;
			break;
		}
	}
#ifdef DEBUG_MEMORY
	free(list);
#endif
	return allow;
}

#include "_usage.h"
#define getoptstring "dDsv" getoptstring_COMMON
#define extraopts "stop | start | restart | describe | zap"
static const struct option longopts[] = {
	{ "debug",      0, NULL, 'd'},
	{ "ifstarted",  0, NULL, 's'},
	{ "nodeps",     0, NULL, 'D'},
	longopts_COMMON
};
static const char *const longopts_help[] = {
	"set xtrace when running the script",
	"only run commands when started",
	"ignore dependencies",
	longopts_help_COMMON
};
#include "_usage.c"

int
runscript(int argc, char **argv)
{
	bool deps = true;
	bool doneone = false;
	char pidstr[10];
	int retval;
	int opt;
	RC_STRING *svc;
	char path[PATH_MAX];
	char lnk[PATH_MAX];
	size_t l = 0;
	size_t ll;
	char *dir, *save = NULL;
	const char *file;
	int depoptions = RC_DEP_TRACE;
	struct stat stbuf;

	/* Show help if insufficient args */
	if (argc < 2 || !exists(argv[1])) {
		fprintf(stderr, "runscript should not be run directly\n");
		exit(EXIT_FAILURE);
	}

	if (stat(argv[1], &stbuf) != 0) {
		fprintf(stderr, "runscript `%s': %s\n",
			argv[1], strerror(errno));
		exit(EXIT_FAILURE);
	}

	atexit(cleanup);

	/* We need to work out the real full path to our service.
	 * This works fine, provided that we ONLY allow mulitplexed services
	 * to exist in the same directory as the master link.
	 * Also, the master link as to be a real file in the init dir. */
	if (!realpath(argv[1], path)) {
		fprintf(stderr, "realpath: %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}
	memset(lnk, 0, sizeof(lnk));
	if (readlink(argv[1], lnk, sizeof(lnk)-1)) {
		dir = dirname(path);
		if (strchr(lnk, '/')) {
			save = xstrdup(dir);
			dir = dirname(lnk);
			if (strcmp(dir, save) == 0)
				file = basename_c(argv[1]);
			else
				file = basename_c(lnk);
			dir = save; 
		} else
			file = basename_c(argv[1]);
		ll = strlen(dir) + strlen(file) + 2;
		service = xmalloc(ll);
		snprintf(service, ll, "%s/%s", dir, file);
		if (stat(service, &stbuf) != 0) {
			free(service);
			service = xstrdup(lnk);
		}
		free(save);
	}
	if (!service)
		service = xstrdup(path);
	applet = basename_c(service);

	if (argc < 3)
		usage(EXIT_FAILURE);

	/* Change dir to / to ensure all init scripts don't use stuff in pwd */
	chdir("/");

	if ((runlevel = xstrdup(getenv("RC_RUNLEVEL"))) == NULL) {
		env_filter();
		env_config();
		runlevel = rc_runlevel_get();
	}

	setenv("EINFO_LOG", service, 1);
	setenv("RC_SVCNAME", applet, 1);

	/* Set an env var so that we always know our pid regardless of any
	   subshells the init script may create so that our mark_service_*
	   functions can always instruct us of this change */
	snprintf(pidstr, sizeof(pidstr), "%d", (int) getpid());
	setenv("RC_RUNSCRIPT_PID", pidstr, 1);

	/* eprefix is kinda klunky, but it works for our purposes */
	if (rc_conf_yesno("rc_parallel")) {
		/* Get the longest service name */
		services = rc_services_in_runlevel(NULL);
		TAILQ_FOREACH(svc, services, entries) {
			ll = strlen(svc->value);
			if (ll > l)
				l = ll;
		}
		rc_stringlist_free(services);
		services = NULL;
		ll = strlen(applet);
		if (ll > l)
			l = ll;

		/* Make our prefix string */
		prefix = xmalloc(sizeof(char) * l + 1);
		ll = strlen(applet);
		memcpy(prefix, applet, ll);
		memset(prefix + ll, ' ', l - ll);
		memset(prefix + l, 0, 1);
		eprefix(prefix);
	}

#ifdef __linux__
	/* Ok, we are ready to go, so setup selinux if applicable */
	setup_selinux(argc, argv);
#endif

	/* Punt the first arg as it's our service name */
	argc--;
	argv++;

	/* Right then, parse any options there may be */
	while ((opt = getopt_long(argc, argv, getoptstring,
				  longopts, (int *)0)) != -1)
		switch (opt) {
		case 'd':
			setenv("RC_DEBUG", "YES", 1);
			break;
		case 's':
			if (!(rc_service_state(service) & RC_SERVICE_STARTED))
				exit(EXIT_FAILURE);
			break;
		case 'D':
			deps = false;
			break;
		case_RC_COMMON_GETOPT
	}

	/* Save the IN_BACKGROUND env flag so it's ONLY passed to the service
	   that is being called and not any dependents */
	if (getenv("IN_BACKGROUND")) {
		ibsave = xstrdup(getenv("IN_BACKGROUND"));
		in_background = rc_yesno(ibsave);
		unsetenv("IN_BACKGROUND");
	}

	if (rc_yesno(getenv("IN_HOTPLUG"))) {
		if (!service_plugable())
			eerrorx("%s: not allowed to be hotplugged", applet);
	}

	/* Setup a signal handler */
	signal_setup(SIGHUP, handle_signal);
	signal_setup(SIGINT, handle_signal);
	signal_setup(SIGQUIT, handle_signal);
	signal_setup(SIGTERM, handle_signal);
	signal_setup(SIGCHLD, handle_signal);

	/* Load our plugins */
	rc_plugin_load();

	applet_list = rc_stringlist_new();
	rc_stringlist_add(applet_list, applet);

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
		unsetenv("RC_CMD");
		setenv("RC_CMD", optarg, 1);

		doneone = true;

		if (strcmp(optarg, "describe") == 0 ||
		    strcmp(optarg, "help") == 0)
		{
			save = prefix;
			eprefix(NULL);
			prefix = NULL;
			svc_exec(optarg, NULL);
			eprefix(save);
			prefix = save;
		} else if (strcmp(optarg, "ineed") == 0 ||
			   strcmp(optarg, "iuse") == 0 ||
			   strcmp(optarg, "needsme") == 0 ||
			   strcmp(optarg, "usesme") == 0 ||
			   strcmp(optarg, "iafter") == 0 ||
			   strcmp(optarg, "ibefore") == 0 ||
			   strcmp(optarg, "iprovide") == 0)
		{
			errno = 0;
			if (rc_conf_yesno("rc_depend_strict") ||
			    errno == ENOENT)
				depoptions |= RC_DEP_STRICT;

			if (!deptree &&
			    ((deptree = _rc_deptree_load(0, NULL)) == NULL))
				eerrorx("failed to load deptree");

			tmplist = rc_stringlist_new();
			rc_stringlist_add(tmplist, optarg);
			services = rc_deptree_depends(deptree, tmplist,
						      applet_list,
						      runlevel, depoptions);
			rc_stringlist_free(tmplist);
			TAILQ_FOREACH(svc, services, entries)
				printf("%s ", svc->value);
			printf ("\n");
			rc_stringlist_free(services);
			services = NULL;
		} else if (strcmp (optarg, "status") == 0) {
			RC_SERVICE r = svc_status();
			retval = (int) r;
			if (retval & RC_SERVICE_STARTED)
				retval = 0;
		} else {
			if (strcmp(optarg, "conditionalrestart") == 0 ||
			    strcmp(optarg, "condrestart") == 0)
			{
				if (rc_service_state(service) &
				    RC_SERVICE_STARTED)
					svc_restart(deps);
			} else if (strcmp(optarg, "restart") == 0) {
				svc_restart (deps);
			} else if (strcmp(optarg, "start") == 0) {
				svc_start(deps);
			} else if (strcmp(optarg, "stop") == 0) {
				if (deps && in_background)
					get_started_services();
				svc_stop(deps);
				if (deps) {
					if (! in_background &&
					    ! rc_runlevel_stopping() &&
					    rc_service_state(service) &
					    RC_SERVICE_STOPPED)
						unhotplug();

					if (in_background &&
					    rc_service_state(service) &
					    RC_SERVICE_INACTIVE)
					{
						TAILQ_FOREACH(svc,
							      restart_services, 
							      entries)
							if (rc_service_state(svc->value) &
							    RC_SERVICE_STOPPED)
								rc_service_schedule_start(service, svc->value);
					}
				}
			} else if (strcmp(optarg, "zap") == 0) {
				einfo("Manually resetting %s to stopped state",
				      applet);
				if (!rc_service_mark(applet,
						     RC_SERVICE_STOPPED))
					eerrorx("rc_service_mark: %s",
						strerror(errno));
				unhotplug();
			} else
				svc_exec(optarg, NULL);

			/* We should ensure this list is empty after
			 * an action is done */
			rc_stringlist_free(restart_services);
			restart_services = NULL;
		}

		if (!doneone)
			usage(EXIT_FAILURE);
	}

	return retval;
}
