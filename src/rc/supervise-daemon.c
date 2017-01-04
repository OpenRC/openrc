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
#include "_usage.h"
#include "helpers.h"

const char *applet = NULL;
const char *extraopts = NULL;
const char *getoptstring = "d:e:g:I:Kk:N:p:r:Su:1:2:" \
	getoptstring_COMMON;
const struct option longopts[] = {
	{ "chdir",        1, NULL, 'd'},
	{ "env",          1, NULL, 'e'},
	{ "group",        1, NULL, 'g'},
	{ "ionice",       1, NULL, 'I'},
	{ "stop",         0, NULL, 'K'},
	{ "umask",        1, NULL, 'k'},
	{ "nicelevel",    1, NULL, 'N'},
	{ "pidfile",      1, NULL, 'p'},
	{ "user",         1, NULL, 'u'},
	{ "chroot",       1, NULL, 'r'},
	{ "start",        0, NULL, 'S'},
	{ "stdout",       1, NULL, '1'},
	{ "stderr",       1, NULL, '2'},
	longopts_COMMON
};
const char * const longopts_help[] = {
	"Change the PWD",
	"Set an environment string",
	"Change the process group",
	"Set an ionice class:data when starting",
	"Stop daemon",
	"Set the umask for the daemon",
	"Set a nicelevel when starting",
	"Match pid found in this file",
	"Change the process user",
	"Chroot to this directory",
	"Start daemon",
	"Redirect stdout to file",
	"Redirect stderr to file",
	longopts_help_COMMON
};
const char *usagestring = NULL;

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
static bool exiting = false;
#ifdef TIOCNOTTY
static int tty_fd = -1;
#endif

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

static pid_t get_pid(const char *pidfile)
{
	FILE *fp;
	pid_t pid;

	if (! pidfile)
		return -1;

	if ((fp = fopen(pidfile, "r")) == NULL) {
		ewarnv("%s: fopen `%s': %s", applet, pidfile, strerror(errno));
		return -1;
	}

	if (fscanf(fp, "%d", &pid) != 1) {
		ewarnv("%s: no pid found in `%s'", applet, pidfile);
		fclose(fp);
		return -1;
	}

	fclose(fp);

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
	char **c;
	char cmdline[PATH_MAX];

#ifdef HAVE_PAM
	pam_handle_t *pamh = NULL;
	int pamr;
	const char *const *pamenv = NULL;
#endif

	setsid();

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
			strncmp(env->value, "RC_SERVICE=", 10) != 0 &&
			strncmp(env->value, "RC_SVCNAME=", 10) != 0) ||
		    strncmp(env->value, "SSD_NICELEVEL=", 14) == 0)
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
		close(i);

	*cmdline = '\0';
	c = argv;
	while (*c) {
		strcat(cmdline, *c);
		strcat(cmdline, " ");
		c++;
	}
	syslog(LOG_INFO, "Running command line: %s", cmdline);
	execvp(exec, argv);

#ifdef HAVE_PAM
	if (changeuser != NULL && pamr == PAM_SUCCESS)
		pam_close_session(pamh, PAM_SILENT);
#endif
	eerrorx("%s: failed to exec `%s': %s", applet, exec,strerror(errno));
}

static void handle_signal(int sig)
{
	int serrno = errno;
	char signame[10] = { '\0' };

	switch (sig) {
	case SIGINT:
		snprintf(signame, sizeof(signame), "SIGINT");
		break;
	case SIGTERM:
		snprintf(signame, sizeof(signame), "SIGTERM");
		break;
	case SIGQUIT:
		snprintf(signame, sizeof(signame), "SIGQUIT");
		break;
	}

	if (*signame != 0) {
		syslog(LOG_INFO, "%s: caught signal %s, exiting", applet, signame);
		exiting = true;
	} else
		syslog(LOG_INFO, "%s: caught unknown signal %d", applet, sig);

	/* Restore errno */
	errno = serrno;
}

static char * expand_home(const char *home, const char *path)
{
	char *opath, *ppath, *p, *nh;
	size_t len;
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

	len = strlen(ppath) + strlen(home) + 1;
	nh = xmalloc(len);
	snprintf(nh, len, "%s%s", home, ppath);
	free(opath);
	return nh;
}

int main(int argc, char **argv)
{
	int opt;
	bool start = false;
	bool stop = false;
	char *exec = NULL;
	char *pidfile = NULL;
	char *home = NULL;
	int tid = 0;
	pid_t child_pid, pid;
	char *svcname = getenv("RC_SVCNAME");
	char *tmp;
	char *p;
	char *token;
	int i;
	char exec_file[PATH_MAX];
	struct passwd *pw;
	struct group *gr;
	FILE *fp;
	mode_t numask = 022;

	applet = basename_c(argv[0]);
	atexit(cleanup);

	signal_setup(SIGINT, handle_signal);
	signal_setup(SIGQUIT, handle_signal);
	signal_setup(SIGTERM, handle_signal);
	openlog(applet, LOG_PID, LOG_DAEMON);

	if ((tmp = getenv("SSD_NICELEVEL")))
		if (sscanf(tmp, "%d", &nicelevel) != 1)
			eerror("%s: invalid nice level `%s' (SSD_NICELEVEL)",
			    applet, tmp);

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

	while ((opt = getopt_long(argc, argv, getoptstring, longopts,
		    (int *) 0)) != -1)
		switch (opt) {
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

		case 'k':
			if (parse_mode(&numask, optarg))
				eerrorx("%s: invalid mode `%s'",
				    applet, optarg);
			break;

		case 'p':  /* --pidfile <pid-file> */
			pidfile = optarg;
			break;

		case 'r':  /* --chroot /new/root */
			ch_root = optarg;
			break;

		case 'u':  /* --user <username>|<uid> */
		{
			p = optarg;
			tmp = strsep(&p, ":");
			changeuser = xstrdup(tmp);
			if (sscanf(tmp, "%d", &tid) != 1)
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
				if (sscanf(tmp, "%d", &tid) != 1)
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

		case_RC_COMMON_GETOPT
		}

	if (!pidfile)
		eerrorx("%s: --pidfile must be specified", applet);

	endpwent();
	argc -= optind;
	argv += optind;
	exec = *argv;

	if (start) {
		if (!exec)
			eerrorx("%s: nothing to start", applet);
	}

	/* Expand ~ */
	if (ch_dir && *ch_dir == '~')
		ch_dir = expand_home(home, ch_dir);
	if (ch_root && *ch_root == '~')
		ch_root = expand_home(home, ch_root);
	if (exec) {
		if (*exec == '~')
			exec = expand_home(home, exec);

		/* Validate that the binary exists if we are starting */
		if (*exec == '/' || *exec == '.') {
			/* Full or relative path */
			if (ch_root)
				snprintf(exec_file, sizeof(exec_file),
				    "%s/%s", ch_root, exec);
			else
				snprintf(exec_file, sizeof(exec_file),
				    "%s", exec);
		} else {
			/* Something in $PATH */
			p = tmp = xstrdup(getenv("PATH"));
			*exec_file = '\0';
			while ((token = strsep(&p, ":"))) {
				if (ch_root)
					snprintf(exec_file, sizeof(exec_file),
					    "%s/%s/%s",
					    ch_root, token, exec);
				else
					snprintf(exec_file, sizeof(exec_file),
					    "%s/%s", token, exec);
				if (exists(exec_file))
					break;
				*exec_file = '\0';
			}
			free(tmp);
		}
	}
	if (start && !exists(exec_file))
		eerrorx("%s: %s does not exist", applet,
		    *exec_file ? exec_file : exec);

	if (stop) {
		pid = get_pid(pidfile);
		if (pid == -1)
			i = pid;
		else
			i = kill(pid, SIGTERM);
		if (i != 0)
			/* We failed to stop something */
			exit(EXIT_FAILURE);

		/* Even if we have not actually killed anything, we should
		 * remove information about it as it may have unexpectedly
		 * crashed out. We should also return success as the end
		 * result would be the same. */
		if (pidfile && exists(pidfile))
			unlink(pidfile);
		if (svcname)
			rc_service_daemon_set(svcname, exec,
			    (const char *const *)argv,
			    pidfile, false);
		exit(EXIT_SUCCESS);
	}

	pid = get_pid(pidfile);
	if (pid != -1)
		if (kill(pid, 0) == 0)
			eerrorx("%s: %s is already running", applet, exec);

	einfov("Detaching to start `%s'", exec);
	eindentv();

	/* Remove existing pidfile */
	if (pidfile)
		unlink(pidfile);

	/*
	 * Make sure we can write a pid file
	 */
	fp = fopen(pidfile, "w");
	if (! fp)
		eerrorx("%s: fopen `%s': %s", applet, pidfile, strerror(errno));
	fclose(fp);

	child_pid = fork();
	if (child_pid == -1)
		eerrorx("%s: fork: %s", applet, strerror(errno));

	/* first parent process, do nothing. */
	if (child_pid != 0)
		exit(EXIT_SUCCESS);

	child_pid = fork();
	if (child_pid == -1)
		eerrorx("%s: fork: %s", applet, strerror(errno));

	if (child_pid != 0) {
		/* this is the supervisor */
		umask(numask);

#ifdef TIOCNOTTY
		tty_fd = open("/dev/tty", O_RDWR);
#endif

		devnull_fd = open("/dev/null", O_RDWR);

		fp = fopen(pidfile, "w");
		if (! fp)
			eerrorx("%s: fopen `%s': %s", applet, pidfile, strerror(errno));
		fprintf(fp, "%d\n", getpid());
		fclose(fp);

		/*
		 * Supervisor main loop
		 */
		i = 0;
		while (!exiting) {
			wait(&i);
			if (exiting) {
				syslog(LOG_INFO, "stopping %s, pid %d", exec, child_pid);
				kill(child_pid, SIGTERM);
			} else {
				if (WIFEXITED(i))
					syslog(LOG_INFO, "%s, pid %d, exited with return code %d",
							exec, child_pid, WEXITSTATUS(i));
				else if (WIFSIGNALED(i))
					syslog(LOG_INFO, "%s, pid %d, terminated by signal %d",
							exec, child_pid, WTERMSIG(i));
				child_pid = fork();
				if (child_pid == -1)
					eerrorx("%s: fork: %s", applet, strerror(errno));
				if (child_pid == 0)
					child_process(exec, argv);
			}
		}

		if (svcname)
			rc_service_daemon_set(svcname, exec,
									(const char * const *) argv, pidfile, true);

		exit(EXIT_SUCCESS);
	} else if (child_pid == 0)
		child_process(exec, argv);
}
