/*
 * supervise-daemon
 * This is an experimental supervisor for daemons.
 * It will start a deamon and make sure it restarts if it crashes.
 */

/*
 * Copyright (c) 2016 The OpenRC Authors.
 * See the Authors file at the top-level directory of this distribution and
 * https://github.com/OpenRC/openrc/blob/master/AUTHORS
 *
 * This file is part of OpenRC. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution and at https://github.com/OpenRC/openrc/blob/master/LICENSE
 * This file may not be copied, modified, propagated, or distributed
 *    except according to the terms contained in the LICENSE file.
 */

/* nano seconds */
#define POLL_INTERVAL   20000000
#define WAIT_PIDFILE   500000000
#define ONE_SECOND    1000000000
#define ONE_MS           1000000

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <termios.h>
#include <sys/time.h>
#include <sys/wait.h>

#ifdef __linux__
#include <sys/syscall.h> /* For io priority */
#endif

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <limits.h>
#include <grp.h>
#include <pwd.h>
#include <signal.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <time.h>
#include <unistd.h>

#ifdef HAVE_PAM
#include <security/pam_appl.h>

/* We are not supporting authentication conversations */
static struct pam_conv conv = { NULL, NULL};
#endif

#include "einfo.h"
#include "queue.h"
#include "rc.h"
#include "rc-misc.h"
#include "rc-plugin.h"
#include "rc-schedules.h"
#include "_usage.h"
#include "helpers.h"

const char *applet = NULL;
const char *extraopts = NULL;
const char *getoptstring = "A:a:D:d:e:g:H:I:Kk:m:N:p:R:r:s:Su:1:2:3" \
	getoptstring_COMMON;
const struct option longopts[] = {
	{ "healthcheck-timer",        1, NULL, 'a'},
	{ "healthcheck-delay",        1, NULL, 'A'},
	{ "respawn-delay",        1, NULL, 'D'},
	{ "chdir",        1, NULL, 'd'},
	{ "env",          1, NULL, 'e'},
	{ "group",        1, NULL, 'g'},
	{ "ionice",       1, NULL, 'I'},
	{ "stop",         0, NULL, 'K'},
	{ "umask",        1, NULL, 'k'},
	{ "respawn-max",    1, NULL, 'm'},
	{ "nicelevel",    1, NULL, 'N'},
	{ "pidfile",      1, NULL, 'p'},
	{ "respawn-period",        1, NULL, 'P'},
	{ "retry",       1, NULL, 'R'},
	{ "chroot",       1, NULL, 'r'},
	{ "signal",       1, NULL, 's'},
	{ "start",        0, NULL, 'S'},
	{ "user",         1, NULL, 'u'},
	{ "stdout",       1, NULL, '1'},
	{ "stderr",       1, NULL, '2'},
	{ "reexec",       0, NULL, '3'},
	longopts_COMMON
};
const char * const longopts_help[] = {
	"set an initial health check delay",
	"set a health check timer",
	"Set a respawn delay",
	"Change the PWD",
	"Set an environment string",
	"Change the process group",
	"Set an ionice class:data when starting",
	"Stop daemon",
	"Set the umask for the daemon",
	"set maximum number of respawn attempts",
	"Set a nicelevel when starting",
	"Match pid found in this file",
	"Set respawn time period",
	"Retry schedule to use when stopping",
	"Chroot to this directory",
	"Send a signal to the daemon",
	"Start daemon",
	"Change the process user",
	"Redirect stdout to file",
	"Redirect stderr to file",
	"reexec (used internally)",
	longopts_help_COMMON
};
const char *usagestring = NULL;

static int healthcheckdelay = 0;
static int healthchecktimer = 0;
static volatile sig_atomic_t do_healthcheck = 0;
static volatile sig_atomic_t exiting = 0;
static int nicelevel = 0;
static int ionicec = -1;
static int ioniced = 0;
static char *changeuser, *ch_root, *ch_dir;
static uid_t uid = 0;
static gid_t gid = 0;
static int devnull_fd = -1;
static int stdin_fd;
static int stdout_fd;
static int stderr_fd;
static char *redirect_stderr = NULL;
static char *redirect_stdout = NULL;
#ifdef TIOCNOTTY
static int tty_fd = -1;
#endif
static pid_t child_pid;
static int respawn_count = 0;
static int respawn_delay = 0;
static int respawn_max = 10;
static int respawn_period = 0;
static char *fifopath = NULL;
static int fifo_fd = 0;
static char *pidfile = NULL;
static char *svcname = NULL;
static bool verbose = false;

extern char **environ;

#if !defined(SYS_ioprio_set) && defined(__NR_ioprio_set)
# define SYS_ioprio_set __NR_ioprio_set
#endif
#if !defined(__DragonFly__)
static inline int ioprio_set(int which _unused, int who _unused,
			     int ioprio _unused)
{
#ifdef SYS_ioprio_set
	return syscall(SYS_ioprio_set, which, who, ioprio);
#else
	return 0;
#endif
}
#endif

static void cleanup(void)
{
	free(changeuser);
}

static void re_exec_supervisor(void)
{
	syslog(LOG_WARNING, "Re-executing for %s", svcname);
	execlp("supervise-daemon", "supervise-daemon", svcname, "--reexec",
			(char *) NULL);
	syslog(LOG_ERR, "Unable to execute supervise-daemon: %s",
			strerror(errno));
	exit(EXIT_FAILURE);
}

static void handle_signal(int sig)
{
	int serrno = errno;
	pid_t pid;

	switch (sig) {
	case SIGALRM:
		do_healthcheck = 1;
		break;
	case SIGCHLD:
		if (exiting)
			while (waitpid((pid_t)(-1), NULL, WNOHANG) > 0) {}
		else {
			while ((pid = waitpid((pid_t)(-1), NULL, WNOHANG|WNOWAIT)) > 0) {
				if (pid == child_pid)
					break;
				pid = waitpid(pid, NULL, WNOHANG);
			}
		}
		break;
	case SIGTERM:
		exiting = 1;
		break;
	default:
		syslog(LOG_WARNING, "caught signal %d", sig);
		re_exec_supervisor();
	}
	/* Restore errno */
	errno = serrno;
}

static char * expand_home(const char *home, const char *path)
{
	char *opath, *ppath, *p, *nh;
	struct passwd *pw;

	if (!path || *path != '~')
		return xstrdup(path);

	opath = ppath = xstrdup(path);
	if (ppath[1] != '/' && ppath[1] != '\0') {
		p = strchr(ppath + 1, '/');
		if (p)
			*p = '\0';
		pw = getpwnam(ppath + 1);
		if (pw) {
			home = pw->pw_dir;
			ppath = p;
			if (ppath)
				*ppath = '/';
		} else
			home = NULL;
	} else
		ppath++;

	if (!home) {
		free(opath);
		return xstrdup(path);
	}
	if (!ppath) {
		free(opath);
		return xstrdup(home);
	}

	xasprintf(&nh, "%s%s", home, ppath);
	free(opath);
	return nh;
}

static char *make_cmdline(char **argv)
{
	char **c;
	char *cmdline = NULL;
	size_t len = 0;

	for (c = argv; c && *c; c++)
		len += (strlen(*c) + 1);
	cmdline = xmalloc(len+1);
	memset(cmdline, 0, len+1);
	for (c = argv; c && *c; c++) {
		strcat(cmdline, *c);
		strcat(cmdline, " ");
	}
	return cmdline;
}

static pid_t exec_command(const char *cmd)
{
	char *file;
	pid_t pid = -1;
	sigset_t full;
	sigset_t old;
	struct sigaction sa;

	file = rc_service_resolve(svcname);
	if (!exists(file)) {
		free(file);
		return 0;
	}

	/* We need to block signals until we have forked */
	memset(&sa, 0, sizeof (sa));
	sa.sa_handler = SIG_DFL;
	sigemptyset(&sa.sa_mask);
	sigfillset(&full);
	sigprocmask(SIG_SETMASK, &full, &old);

	pid = fork();
	if (pid == 0) {
		/* Restore default handlers */
		sigaction(SIGCHLD, &sa, NULL);
		sigaction(SIGHUP, &sa, NULL);
		sigaction(SIGINT, &sa, NULL);
		sigaction(SIGQUIT, &sa, NULL);
		sigaction(SIGTERM, &sa, NULL);
		sigaction(SIGUSR1, &sa, NULL);
		sigaction(SIGWINCH, &sa, NULL);

		/* Unmask signals */
		sigprocmask(SIG_SETMASK, &old, NULL);

		/* Safe to run now */
		execl(file, file, cmd, (char *) NULL);
		syslog(LOG_ERR, "unable to exec `%s': %s\n",
		    file, strerror(errno));
		_exit(EXIT_FAILURE);
	}

	if (pid == -1)
		syslog(LOG_ERR, "fork: %s\n",strerror (errno));

	sigprocmask(SIG_SETMASK, &old, NULL);
	free(file);
	return pid;
}

static void child_process(char *exec, char **argv)
{
	RC_STRINGLIST *env_list;
	RC_STRING *env;
	int i;
	char *p;
	char *token;
	size_t len;
	char *newpath;
	char *np;
	char *cmdline = NULL;
	time_t start_time;
	char start_count_string[20];
	char start_time_string[20];

#ifdef HAVE_PAM
	pam_handle_t *pamh = NULL;
	int pamr;
	const char *const *pamenv = NULL;
#endif

	setsid();

	if (svcname) {
		start_time = time(NULL);
		from_time_t(start_time_string, start_time);
		rc_service_value_set(svcname, "start_time", start_time_string);
		sprintf(start_count_string, "%i", respawn_count);
		rc_service_value_set(svcname, "start_count", start_count_string);
		sprintf(start_count_string, "%d", getpid());
		rc_service_value_set(svcname, "child_pid", start_count_string);
	}

	if (nicelevel) {
		if (setpriority(PRIO_PROCESS, getpid(), nicelevel) == -1)
			eerrorx("%s: setpriority %d: %s", applet, nicelevel,
					strerror(errno));
	}

	if (ionicec != -1 && ioprio_set(1, getpid(), ionicec | ioniced) == -1)
		eerrorx("%s: ioprio_set %d %d: %s", applet, ionicec, ioniced,
				strerror(errno));

	if (ch_root && chroot(ch_root) < 0)
		eerrorx("%s: chroot `%s': %s", applet, ch_root, strerror(errno));

	if (ch_dir && chdir(ch_dir) < 0)
		eerrorx("%s: chdir `%s': %s", applet, ch_dir, strerror(errno));

#ifdef HAVE_PAM
	if (changeuser != NULL) {
		pamr = pam_start("supervise-daemon",
		    changeuser, &conv, &pamh);

		if (pamr == PAM_SUCCESS)
			pamr = pam_acct_mgmt(pamh, PAM_SILENT);
		if (pamr == PAM_SUCCESS)
			pamr = pam_open_session(pamh, PAM_SILENT);
		if (pamr != PAM_SUCCESS)
			eerrorx("%s: pam error: %s", applet, pam_strerror(pamh, pamr));
	}
#endif

	if (gid && setgid(gid))
		eerrorx("%s: unable to set groupid to %d", applet, gid);
	if (changeuser && initgroups(changeuser, gid))
		eerrorx("%s: initgroups (%s, %d)", applet, changeuser, gid);
	if (uid && setuid(uid))
		eerrorx ("%s: unable to set userid to %d", applet, uid);

	/* Close any fd's to the passwd database */
	endpwent();

	/* remove the controlling tty */
#ifdef TIOCNOTTY
	ioctl(tty_fd, TIOCNOTTY, 0);
	close(tty_fd);
#endif

	/* Clean the environment of any RC_ variables */
	env_list = rc_stringlist_new();
	i = 0;
	while (environ[i])
		rc_stringlist_add(env_list, environ[i++]);

#ifdef HAVE_PAM
	if (changeuser != NULL) {
		pamenv = (const char *const *)pam_getenvlist(pamh);
		if (pamenv) {
			while (*pamenv) {
				/* Don't add strings unless they set a var */
				if (strchr(*pamenv, '='))
					putenv(xstrdup(*pamenv));
				else
					unsetenv(*pamenv);
				pamenv++;
			}
		}
	}
#endif

	TAILQ_FOREACH(env, env_list, entries) {
		if ((strncmp(env->value, "RC_", 3) == 0 &&
			strncmp(env->value, "RC_SERVICE=", 11) != 0 &&
			strncmp(env->value, "RC_SVCNAME=", 11) != 0) ||
		    strncmp(env->value, "SSD_NICELEVEL=", 14) == 0 ||
		    strncmp(env->value, "SSD_IONICELEVEL=", 16) == 0)
		{
			p = strchr(env->value, '=');
			*p = '\0';
			unsetenv(env->value);
			continue;
		}
	}
	rc_stringlist_free(env_list);

	/* For the path, remove the rcscript bin dir from it */
	if ((token = getenv("PATH"))) {
		len = strlen(token);
		newpath = np = xmalloc(len + 1);
		while (token && *token) {
			p = strchr(token, ':');
			if (p) {
				*p++ = '\0';
				while (*p == ':')
					p++;
			}
			if (strcmp(token, RC_LIBEXECDIR "/bin") != 0 &&
			    strcmp(token, RC_LIBEXECDIR "/sbin") != 0)
			{
				len = strlen(token);
				if (np != newpath)
					*np++ = ':';
				memcpy(np, token, len);
				np += len;
				}
			token = p;
		}
		*np = '\0';
		unsetenv("PATH");
		setenv("PATH", newpath, 1);
	}

	stdin_fd = devnull_fd;
	stdout_fd = devnull_fd;
	stderr_fd = devnull_fd;
	if (redirect_stdout) {
		if ((stdout_fd = open(redirect_stdout,
			    O_WRONLY | O_CREAT | O_APPEND,
			    S_IRUSR | S_IWUSR)) == -1)
			eerrorx("%s: unable to open the logfile"
				    " for stdout `%s': %s",
				    applet, redirect_stdout, strerror(errno));
	}
	if (redirect_stderr) {
		if ((stderr_fd = open(redirect_stderr,
			    O_WRONLY | O_CREAT | O_APPEND,
			    S_IRUSR | S_IWUSR)) == -1)
			eerrorx("%s: unable to open the logfile"
			    " for stderr `%s': %s",
			    applet, redirect_stderr, strerror(errno));
	}

	dup2(stdin_fd, STDIN_FILENO);
	if (redirect_stdout || rc_yesno(getenv("EINFO_QUIET")))
		dup2(stdout_fd, STDOUT_FILENO);
	if (redirect_stderr || rc_yesno(getenv("EINFO_QUIET")))
		dup2(stderr_fd, STDERR_FILENO);

	for (i = getdtablesize() - 1; i >= 3; --i)
		fcntl(i, F_SETFD, FD_CLOEXEC);
	cmdline = make_cmdline(argv);
	syslog(LOG_INFO, "Child command line: %s", cmdline);
	free(cmdline);
	execvp(exec, argv);

#ifdef HAVE_PAM
	if (changeuser != NULL && pamr == PAM_SUCCESS)
		pam_close_session(pamh, PAM_SILENT);
#endif
	eerrorx("%s: failed to exec `%s': %s", applet, exec,strerror(errno));
}

static void supervisor(char *exec, char **argv)
{
	FILE *fp;
	char buf[2048];
	char cmd[2048];
	int count;
	int failing;
	int health_status;
	int healthcheck_respawn;
	int i;
	int nkilled;
	int sig_send;
	pid_t health_pid;
	pid_t wait_pid;
	sigset_t old_signals;
	sigset_t signals;
	struct sigaction sa;
	struct timespec ts;
	time_t respawn_now= 0;
	time_t first_spawn= 0;

	/* block all signals we do not handle */
	sigfillset(&signals);
	sigdelset(&signals, SIGALRM);
	sigdelset(&signals, SIGCHLD);
	sigdelset(&signals, SIGTERM);
	sigprocmask(SIG_SETMASK, &signals, &old_signals);

	/* install signal  handler */
	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = handle_signal;
	sigaction(SIGALRM, &sa, NULL);
	sigaction(SIGCHLD, &sa, NULL);
	sigaction(SIGTERM, &sa, NULL);

	fp = fopen(pidfile, "w");
	if (!fp)
		eerrorx("%s: fopen `%s': %s", applet, pidfile, strerror(errno));
	fprintf(fp, "%d\n", getpid());
	fclose(fp);

	if (svcname)
		rc_service_daemon_set(svcname, exec, (const char * const *) argv,
				pidfile, true);

	/* remove the controlling tty */
#ifdef TIOCNOTTY
	ioctl(tty_fd, TIOCNOTTY, 0);
	close(tty_fd);
#endif

	/*
	 * Supervisor main loop
	 */
	if (healthcheckdelay)
		alarm(healthcheckdelay);
	else if (healthchecktimer)
		alarm(healthchecktimer);
	failing = 0;
	while (!exiting) {
		healthcheck_respawn = 0;
		fifo_fd = open(fifopath, O_RDONLY);
		if (fifo_fd > 0) {
			memset(buf, 0, sizeof(buf));
			count = read(fifo_fd, buf, sizeof(buf) - 1);
			close(fifo_fd);
			if (count != -1)
				buf[count] = 0;
			if (count == 0)
				continue;
			if (verbose)
				syslog(LOG_DEBUG, "Received %s from fifo", buf);
			if (strncasecmp(buf, "sig", 3) == 0) {
				if ((sscanf(buf, "%s %d", cmd, &sig_send) == 2)
						&& (sig_send >= 0 && sig_send < NSIG)) {
					syslog(LOG_INFO, "Sending signal %d to %d", sig_send,
							child_pid);
					if (kill(child_pid, sig_send) == -1)
						syslog(LOG_ERR, "Unable to send signal %d to %d",
								sig_send, child_pid);
				}
			}
			continue;
		}
		if (do_healthcheck) {
			do_healthcheck = 0;
			alarm(0);
			if (verbose)
				syslog(LOG_DEBUG, "running health check for %s", svcname);
			health_pid = exec_command("healthcheck");
			health_status = rc_waitpid(health_pid);
			if (WIFEXITED(health_status) && WEXITSTATUS(health_status) == 0)
				alarm(healthchecktimer);
			else {
				syslog(LOG_WARNING, "health check for %s failed", svcname);
				health_pid = exec_command("unhealthy");
				rc_waitpid(health_pid);
				syslog(LOG_INFO, "stopping %s, pid %d", exec, child_pid);
				nkilled = run_stop_schedule(applet, NULL, NULL, child_pid, 0,
						false, false, true);
				if (nkilled < 0)
					syslog(LOG_INFO, "Unable to kill %d: %s",
							child_pid, strerror(errno));
				else
					healthcheck_respawn = 1;
			}
		}
		if (exiting) {
			alarm(0);
			syslog(LOG_INFO, "stopping %s, pid %d", exec, child_pid);
			nkilled = run_stop_schedule(applet, NULL, NULL, child_pid, 0,
					false, false, true);
			if (nkilled > 0)
				syslog(LOG_INFO, "killed %d processes", nkilled);
			continue;
		}
		wait_pid = waitpid(child_pid, &i, WNOHANG);
		if (wait_pid == child_pid) {
			if (WIFEXITED(i))
				syslog(LOG_WARNING, "%s, pid %d, exited with return code %d",
						exec, child_pid, WEXITSTATUS(i));
			else if (WIFSIGNALED(i))
				syslog(LOG_WARNING, "%s, pid %d, terminated by signal %d",
						exec, child_pid, WTERMSIG(i));
		}
		if (wait_pid == child_pid || healthcheck_respawn) {
			do_healthcheck = 0;
			healthcheck_respawn = 0;
			alarm(0);
			respawn_now = time(NULL);
			if (first_spawn == 0)
				first_spawn = respawn_now;
			if ((respawn_period > 0)
					&& (respawn_now - first_spawn > respawn_period)) {
				respawn_count = 0;
				first_spawn = 0;
			} else
				respawn_count++;
			if (respawn_max > 0 && respawn_count > respawn_max) {
				syslog(LOG_WARNING, "respawned \"%s\" too many times, exiting",
						exec);
				exiting = 1;
				failing = 1;
				continue;
			}
			ts.tv_sec = respawn_delay;
			ts.tv_nsec = 0;
			nanosleep(&ts, NULL);
			if (exiting)
				continue;
			child_pid = fork();
			if (child_pid == -1) {
				syslog(LOG_ERR, "%s: fork: %s", applet, strerror(errno));
				exit(EXIT_FAILURE);
			}
			if (child_pid == 0) {
				sigprocmask(SIG_SETMASK, &old_signals, NULL);
				memset(&sa, 0, sizeof(sa));
				sa.sa_handler = SIG_DFL;
				sigaction(SIGALRM, &sa, NULL);
				sigaction(SIGCHLD, &sa, NULL);
				sigaction(SIGTERM, &sa, NULL);
				child_process(exec, argv);
			}
			if (healthcheckdelay)
				alarm(healthcheckdelay);
			else if (healthchecktimer)
				alarm(healthchecktimer);
		}
	}

	if (svcname) {
		rc_service_daemon_set(svcname, exec, (const char *const *)argv,
				pidfile, false);
		rc_service_value_set(svcname, "child_pid", NULL);
		rc_service_mark(svcname, RC_SERVICE_STOPPED);
		if (failing)
			rc_service_mark(svcname, RC_SERVICE_FAILED);
	}
	if (pidfile && exists(pidfile))
		unlink(pidfile);
	if (fifopath && exists(fifopath))
		unlink(fifopath);
	exit(EXIT_SUCCESS);
}

int main(int argc, char **argv)
{
	int opt;
	char **c;
	int x;
	bool start = false;
	bool stop = false;
	bool reexec = false;
	bool sendsig = false;
	char *exec = NULL;
	char *retry = NULL;
	int sig = SIGTERM;
	char *home = NULL;
	int tid = 0;
	pid_t pid;
	char *tmp;
	char *p;
	char *token;
	int i;
	int n;
	char *exec_file = NULL;
	char *varbuf = NULL;
	struct timespec ts;
	struct passwd *pw;
	struct group *gr;
	FILE *fp;
	mode_t numask = 022;
	int child_argc = 0;
	char **child_argv = NULL;
	char *str = NULL;
	char *cmdline = NULL;

	applet = basename_c(argv[0]);
	atexit(cleanup);
	svcname = getenv("RC_SVCNAME");
	if (!svcname)
		eerrorx("%s: The RC_SVCNAME environment variable is not set", applet);
	openlog(applet, LOG_PID, LOG_DAEMON);

	if (argc <= 1 || strcmp(argv[1], svcname))
		eerrorx("%s: the first argument is %s and must be %s",
				applet, argv[1], svcname);

	if ((tmp = getenv("SSD_NICELEVEL")))
		if (sscanf(tmp, "%d", &nicelevel) != 1)
			eerror("%s: invalid nice level `%s' (SSD_NICELEVEL)",
			    applet, tmp);
	if ((tmp = getenv("SSD_IONICELEVEL"))) {
		int n = sscanf(tmp, "%d:%d", &ionicec, &ioniced);
		if (n != 1 && n != 2)
			eerror("%s: invalid ionice level `%s' (SSD_IONICELEVEL)",
			    applet, tmp);
		if (ionicec == 0)
			ioniced = 0;
		else if (ionicec == 3)
			ioniced = 7;
		ionicec <<= 13; /* class shift */
	}

	/* Get our user name and initial dir */
	p = getenv("USER");
	home = getenv("HOME");
	if (home == NULL || p == NULL) {
		pw = getpwuid(getuid());
		if (pw != NULL) {
			if (p == NULL)
				setenv("USER", pw->pw_name, 1);
			if (home == NULL) {
				setenv("HOME", pw->pw_dir, 1);
				home = pw->pw_dir;
			}
		}
	}

	cmdline = make_cmdline(argv);
	if (svcname) {
		argc--;
		argv++;
	}
	while ((opt = getopt_long(argc, argv, getoptstring, longopts,
		    (int *) 0)) != -1)
		switch (opt) {
		case 'a':  /* --healthcheck-timer <time> */
			if (sscanf(optarg, "%d", &healthchecktimer) != 1 || healthchecktimer < 1)
				eerrorx("%s: invalid health check timer %s", applet, optarg);
			break;

		case 'A':  /* --healthcheck-delay <time> */
			if (sscanf(optarg, "%d", &healthcheckdelay) != 1 || healthcheckdelay < 1)
				eerrorx("%s: invalid health check delay %s", applet, optarg);
			break;

		case 'D':  /* --respawn-delay time */
			n = sscanf(optarg, "%d", &respawn_delay);
			if (n	!= 1 || respawn_delay < 1)
				eerrorx("Invalid respawn-delay value '%s'", optarg);
			break;

		case 'I': /* --ionice */
			if (sscanf(optarg, "%d:%d", &ionicec, &ioniced) == 0)
				eerrorx("%s: invalid ionice `%s'",
				    applet, optarg);
			if (ionicec == 0)
				ioniced = 0;
			else if (ionicec == 3)
				ioniced = 7;
			ionicec <<= 13; /* class shift */
			break;

		case 'K':  /* --stop */
			stop = true;
			break;

		case 'N':  /* --nice */
			if (sscanf(optarg, "%d", &nicelevel) != 1)
				eerrorx("%s: invalid nice level `%s'",
				    applet, optarg);
			break;

		case 'P':  /* --respawn-period time */
			n = sscanf(optarg, "%d", &respawn_period);
			if (n	!= 1 || respawn_period < 1)
				eerrorx("Invalid respawn-period value '%s'", optarg);
			break;

		case 's':  /* --signal */
			sig = parse_signal(applet, optarg);
			sendsig = true;
			break;
		case 'S':  /* --start */
			start = true;
			break;

		case 'd':  /* --chdir /new/dir */
			ch_dir = optarg;
			break;

		case 'e': /* --env */
			putenv(optarg);
			break;

		case 'g':  /* --group <group>|<gid> */
			if (sscanf(optarg, "%d", &tid) != 1)
				gr = getgrnam(optarg);
			else
				gr = getgrgid((gid_t)tid);
			if (gr == NULL)
				eerrorx("%s: group `%s' not found",
				    applet, optarg);
			gid = gr->gr_gid;
			break;

		case 'H':  /* --healthcheck-timer <minutes> */
			if (sscanf(optarg, "%d", &healthchecktimer) != 1 || healthchecktimer < 1)
				eerrorx("%s: invalid health check timer %s", applet, optarg);
			break;

		case 'k':
			if (parse_mode(&numask, optarg))
				eerrorx("%s: invalid mode `%s'",
				    applet, optarg);
			break;

		case 'm':  /* --respawn-max count */
			n = sscanf(optarg, "%d", &respawn_max);
			if (n	!= 1 || respawn_max < 0)
				eerrorx("Invalid respawn-max value '%s'", optarg);
			break;

		case 'p':  /* --pidfile <pid-file> */
			pidfile = optarg;
			break;

		case 'R':  /* --retry <schedule>|timeout */
			retry = optarg;
			break;
		case 'r':  /* --chroot /new/root */
			ch_root = optarg;
			break;

		case 'u':  /* --user <username>|<uid> */
		{
			char dummy[2];
			p = optarg;
			tmp = strsep(&p, ":");
			changeuser = xstrdup(tmp);
			if (sscanf(tmp, "%d%1s", &tid, dummy) != 1)
				pw = getpwnam(tmp);
			else
				pw = getpwuid((uid_t)tid);

			if (pw == NULL)
				eerrorx("%s: user `%s' not found",
				    applet, tmp);
			uid = pw->pw_uid;
			home = pw->pw_dir;
			unsetenv("HOME");
			if (pw->pw_dir)
				setenv("HOME", pw->pw_dir, 1);
			unsetenv("USER");
			if (pw->pw_name)
				setenv("USER", pw->pw_name, 1);
			if (gid == 0)
				gid = pw->pw_gid;

			if (p) {
				tmp = strsep (&p, ":");
				if (sscanf(tmp, "%d%1s", &tid, dummy) != 1)
					gr = getgrnam(tmp);
				else
					gr = getgrgid((gid_t) tid);

				if (gr == NULL)
					eerrorx("%s: group `%s'"
					    " not found",
					    applet, tmp);
				gid = gr->gr_gid;
			}
		}
		break;

		case '1':   /* --stdout /path/to/stdout.lgfile */
			redirect_stdout = optarg;
			break;

		case '2':  /* --stderr /path/to/stderr.logfile */
			redirect_stderr = optarg;
			break;
		case '3':  /* --reexec */
			reexec = true;
			break;

		case_RC_COMMON_GETOPT
		}

	verbose = rc_yesno(getenv ("EINFO_VERBOSE"));
	endpwent();
	argc -= optind;
	argv += optind;
	exec = *argv;

	/* Expand ~ */
	if (ch_dir && *ch_dir == '~')
		ch_dir = expand_home(home, ch_dir);
	if (ch_root && *ch_root == '~')
		ch_root = expand_home(home, ch_root);

	umask(numask);
	if (!pidfile)
		xasprintf(&pidfile, "/var/run/supervise-%s.pid", svcname);
	xasprintf(&fifopath, "%s/supervise-%s.ctl", RC_SVCDIR, svcname);
	if (mkfifo(fifopath, 0600) == -1 && errno != EEXIST)
		eerrorx("%s: unable to create control fifo: %s",
				applet, strerror(errno));

	if (reexec) {
		str = rc_service_value_get(svcname, "argc");
		sscanf(str, "%d", &child_argc);
		child_argv = xmalloc((child_argc + 1) * sizeof(char *));
		memset(child_argv, 0, (child_argc + 1) * sizeof(char *));
		for (x = 0; x < child_argc; x++) {
			xasprintf(&varbuf, "argv_%d", x);
			str = rc_service_value_get(svcname, varbuf);
			child_argv[x] = str;
			free(varbuf);
			varbuf = NULL;
		}
		free(str);
		str = rc_service_value_get(svcname, "child_pid");
		sscanf(str, "%d", &child_pid);
		free(str);
		exec = rc_service_value_get(svcname, "exec");
		pidfile = rc_service_value_get(svcname, "pidfile");
		retry = rc_service_value_get(svcname, "retry");
		if (retry) {
			parse_schedule(applet, retry, sig);
			rc_service_value_set(svcname, "retry", retry);
		} else
			parse_schedule(applet, NULL, sig);

		str = rc_service_value_get(svcname, "respawn_delay");
		sscanf(str, "%d", &respawn_delay);
		str = rc_service_value_get(svcname, "respawn_max");
		sscanf(str, "%d", &respawn_max);
		supervisor(exec, child_argv);
	} else if (start) {
		if (exec) {
			if (*exec == '~')
				exec = expand_home(home, exec);

			/* Validate that the binary exists if we are starting */
			if (*exec == '/' || *exec == '.') {
				/* Full or relative path */
				if (ch_root)
					xasprintf(&exec_file, "%s/%s", ch_root, exec);
				else
					xasprintf(&exec_file, "%s", exec);
			} else {
				/* Something in $PATH */
				p = tmp = xstrdup(getenv("PATH"));
				exec_file = NULL;
				while ((token = strsep(&p, ":"))) {
					if (ch_root)
						xasprintf(&exec_file, "%s/%s/%s", ch_root, token, exec);
					else
						xasprintf(&exec_file, "%s/%s", token, exec);
					if (exec_file && exists(exec_file))
						break;
					free(exec_file);
					exec_file = NULL;
				}
				free(tmp);
			}
			if (!exists(exec_file)) {
				eerror("%s: %s does not exist", applet,
				    exec_file ? exec_file : exec);
				free(exec_file);
				exit(EXIT_FAILURE);
			}
		} else
			eerrorx("%s: nothing to start", applet);

		pid = get_pid(applet, pidfile);
		if (pid != -1)
			if (do_stop(applet, exec, (const char * const *)argv, pid, uid,
						0, false, true) > 0)
				eerrorx("%s: %s is already running", applet, exec);

		if (respawn_period > 0 && respawn_delay * respawn_max > respawn_period)
			ewarn("%s: Please increase the value of --respawn-period to more "
				"than %d to avoid infinite respawning", applet,
				respawn_delay * respawn_max);

		if (retry) {
			parse_schedule(applet, retry, sig);
			rc_service_value_set(svcname, "retry", retry);
		} else
			parse_schedule(applet, NULL, sig);

		einfov("Detaching to start `%s'", exec);
		syslog(LOG_INFO, "Supervisor command line: %s", cmdline);
		free(cmdline);
		cmdline = NULL;

		/* Remove existing pidfile */
		if (pidfile)
			unlink(pidfile);

		/* Make sure we can write a pid file */
		fp = fopen(pidfile, "w");
		if (! fp)
			eerrorx("%s: fopen `%s': %s", applet, pidfile, strerror(errno));
		fclose(fp);

		rc_service_value_set(svcname, "pidfile", pidfile);
		varbuf = NULL;
		xasprintf(&varbuf, "%i", respawn_delay);
		rc_service_value_set(svcname, "respawn_delay", varbuf);
		free(varbuf);
		xasprintf(&varbuf, "%i", respawn_max);
		rc_service_value_set(svcname, "respawn_max", varbuf);
		free(varbuf);
		xasprintf(&varbuf, "%i", respawn_period);
		rc_service_value_set(svcname, "respawn_period", varbuf);
		free(varbuf);
		child_pid = fork();
		if (child_pid == -1)
			eerrorx("%s: fork: %s", applet, strerror(errno));
		if (child_pid != 0)
			/* first parent process, do nothing. */
			exit(EXIT_SUCCESS);
#ifdef TIOCNOTTY
		tty_fd = open("/dev/tty", O_RDWR);
#endif
		devnull_fd = open("/dev/null", O_RDWR);
		dup2(devnull_fd, STDIN_FILENO);
		dup2(devnull_fd, STDOUT_FILENO);
		dup2(devnull_fd, STDERR_FILENO);
		child_pid = fork();
		if (child_pid == -1)
			eerrorx("%s: fork: %s", applet, strerror(errno));
		else if (child_pid != 0) {
			c = argv;
			x = 0;
			while (c && *c) {
				varbuf = NULL;
				xasprintf(&varbuf, "argv_%-d",x);
				rc_service_value_set(svcname, varbuf, *c);
				free(varbuf);
				varbuf = NULL;
				x++;
				c++;
			}
			xasprintf(&varbuf, "%d", x);
			rc_service_value_set(svcname, "argc", varbuf);
			free(varbuf);
			rc_service_value_set(svcname, "exec", exec);
			supervisor(exec, argv);
		} else
			child_process(exec, argv);
	} else if (stop) {
		pid = get_pid(applet, pidfile);
		if (pid != -1) {
			i = kill(pid, SIGTERM);
			if (i != 0)
				/* We failed to send the signal */
				ewarn("Unable to shut down the supervisor");
			else {
				/* wait for the supervisor to go down */
				while (kill(pid, 0) == 0) {
					ts.tv_sec = 0;
					ts.tv_nsec = 1;
					nanosleep(&ts, NULL);
				}
			}
		}

		/* Even if we have not actually killed anything, we should
		 * remove information about it as it may have unexpectedly
		 * crashed out. We should also return success as the end
		 * result would be the same. */
		if (pidfile && exists(pidfile))
			unlink(pidfile);
		if (svcname) {
			rc_service_daemon_set(svcname, exec,
			    (const char *const *)argv,
			    pidfile, false);
			rc_service_mark(svcname, RC_SERVICE_STOPPED);
		}
		exit(EXIT_SUCCESS);
	} else if (sendsig) {
		fifo_fd = open(fifopath, O_WRONLY |O_NONBLOCK);
		if (fifo_fd < 0)
			eerrorx("%s: unable to open control fifo %s", applet, strerror(errno));
		xasprintf(&str, "sig %d", sig);
		x = write(fifo_fd, str, strlen(str));
		if (x == -1) {
			free(tmp);
			eerrorx("%s: error writing to control fifo: %s", applet,
					strerror(errno));
		}
		free(tmp);
		exit(EXIT_SUCCESS);
	}
}
