/*
  start-stop-daemon
 * Starts, stops, tests and signals daemons
 *
 * This is essentially a ground up re-write of Debian's
 * start-stop-daemon for cleaner code and to integrate into our RC
 * system so we can monitor daemons a little.
 */

/*
 * Copyright (c) 2007-2015 The OpenRC Authors.
 * See the Authors file at the top-level directory of this distribution and
 * https://github.com/OpenRC/openrc/blob/HEAD/AUTHORS
 *
 * This file is part of OpenRC. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution and at https://github.com/OpenRC/openrc/blob/HEAD/LICENSE
 * This file may not be copied, modified, propagated, or distributed
 *    except according to the terms contained in the LICENSE file.
 */

#define ONE_MS           1000000

#if defined(__linux__) && !defined(_GNU_SOURCE)
/* For extra SCHED_* defines. */
# define _GNU_SOURCE
#endif

#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <limits.h>
#include <grp.h>
#include <pthread.h>
#include <pwd.h>
#include <sched.h>
#include <signal.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <sys/ioctl.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#ifdef __linux__
# include <sys/syscall.h> /* For io priority */
# include <sys/prctl.h> /* For prctl */
#endif
#include <termios.h>
#include <time.h>
#include <unistd.h>

#ifdef HAVE_PAM
# include <security/pam_appl.h>

/* We are not supporting authentication conversations */
static struct pam_conv conv = { NULL, NULL};
#endif

#ifdef __linux__
# include <sys/capability.h>
#endif

#include "einfo.h"
#include "queue.h"
#include "rc.h"
#include "misc.h"
#include "rc_exec.h"
#include "schedules.h"
#include "_usage.h"
#include "helpers.h"

/* Use long option value that is out of range for 8 bit getopt values.
 * The exact enum value is internal and can freely change, so we keep the
 * options sorted.
 */
enum {
  /* This has to come first so following values stay in the 0x100+ range. */
  LONGOPT_BASE = 0x100,

  LONGOPT_CAPABILITIES,
  LONGOPT_OOM_SCORE_ADJ,
  LONGOPT_NO_NEW_PRIVS,
  LONGOPT_SCHEDULER,
  LONGOPT_SCHEDULER_PRIO,
  LONGOPT_SECBITS,
  LONGOPT_NOTIFY,
};

const char *applet = NULL;
const char *extraopts = NULL;
const char getoptstring[] = "I:KN:PR:Sa:bc:d:e:g:ik:mn:op:s:tu:r:w:x:0:1:2:3:4:" \
	getoptstring_COMMON;
const struct option longopts[] = {
	{ "capabilities", 1, NULL, LONGOPT_CAPABILITIES},
	{ "secbits",      1, NULL, LONGOPT_SECBITS},
	{ "no-new-privs", 0, NULL, LONGOPT_NO_NEW_PRIVS},
	{ "ionice",       1, NULL, 'I'},
	{ "stop",         0, NULL, 'K'},
	{ "nicelevel",    1, NULL, 'N'},
	{ "oom-score-adj",1, NULL, LONGOPT_OOM_SCORE_ADJ},
	{ "retry",        1, NULL, 'R'},
	{ "start",        0, NULL, 'S'},
	{ "startas",      1, NULL, 'a'},
	{ "background",   0, NULL, 'b'},
	{ "chuid",        1, NULL, 'c'},
	{ "chdir",        1, NULL, 'd'},
	{ "env",          1, NULL, 'e'},
	{ "umask",        1, NULL, 'k'},
	{ "group",        1, NULL, 'g'},
	{ "interpreted",  0, NULL, 'i'},
	{ "make-pidfile", 0, NULL, 'm'},
	{ "name",         1, NULL, 'n'},
	{ "oknodo",       0, NULL, 'o'},
	{ "pidfile",      1, NULL, 'p'},
	{ "signal",       1, NULL, 's'},
	{ "test",         0, NULL, 't'},
	{ "user",         1, NULL, 'u'},
	{ "chroot",       1, NULL, 'r'},
	{ "wait",         1, NULL, 'w'},
	{ "exec",         1, NULL, 'x'},
	{ "stdin",        1, NULL, '0'},
	{ "stdout",       1, NULL, '1'},
	{ "stderr",       1, NULL, '2'},
	{ "stdout-logger",1, NULL, '3'},
	{ "stderr-logger",1, NULL, '4'},
	{ "progress",     0, NULL, 'P'},
	{ "scheduler",    1, NULL, LONGOPT_SCHEDULER},
	{ "scheduler-priority",    1, NULL, LONGOPT_SCHEDULER_PRIO},
	{ "notify",        1, NULL, LONGOPT_NOTIFY},
	longopts_COMMON
};
const char * const longopts_help[] = {
	"Set the inheritable, ambient and bounding capabilities",
	"Set the security-bits for the program",
	"Set the No New Privs flag for the program",
	"Set an ionice class:data when starting",
	"Stop daemon",
	"Set a nicelevel when starting",
	"Set OOM score adjustment when starting",
	"Retry schedule to use when stopping",
	"Start daemon",
	"deprecated, use --exec or --name",
	"Force daemon to background",
	"deprecated, use --user",
	"Change the PWD",
	"Set an environment string",
	"Set the umask for the daemon",
	"Change the process group",
	"Match process name by interpreter",
	"Create a pidfile",
	"Match process name",
	"deprecated",
	"Match pid found in this file",
	"Send a different signal",
	"Test actions, don't do them",
	"Change the process user",
	"Chroot to this directory",
	"Milliseconds to wait for daemon start",
	"Binary to start/stop",
	"Redirect stdin to file",
	"Redirect stdout to file",
	"Redirect stderr to file",
	"Redirect stdout to process",
	"Redirect stderr to process",
	"Print dots each second while waiting",
	"Set process scheduler",
	"Set process scheduler priority",
	"Configures experimental notification behaviour",
	longopts_help_COMMON
};
const char *usagestring = NULL;

static char **nav;

static char *changeuser, *ch_root, *ch_dir;

extern char **environ;

#if !defined(SYS_ioprio_set) && defined(__NR_ioprio_set)
# define SYS_ioprio_set __NR_ioprio_set
#endif
#if !defined(__DragonFly__)
static inline int ioprio_set(int which RC_UNUSED,
			     int who RC_UNUSED,
			     int ioprio RC_UNUSED)
{
#ifdef SYS_ioprio_set
	return syscall(SYS_ioprio_set, which, who, ioprio);
#else
	return 0;
#endif
}
#endif

static void
cleanup(void)
{
	free(changeuser);
	free(nav);
	free_schedulelist();
}

static void
handle_signal(int sig)
{
	int status;
	int serrno = errno;
	const char *signame = NULL;

	switch (sig) {
	case SIGINT:
		if (!signame)
			signame = "SIGINT";
		/* FALLTHROUGH */
	case SIGTERM:
		if (!signame)
			signame = "SIGTERM";
		/* FALLTHROUGH */
	case SIGQUIT:
		if (!signame)
			signame = "SIGQUIT";
		eerrorx("%s: caught %s, aborting", applet, signame);
		/* NOTREACHED */

	case SIGCHLD:
		for (;;) {
			if (waitpid(-1, &status, WNOHANG) < 0) {
				if (errno != ECHILD)
					eerror("%s: waitpid: %s",
					    applet, strerror(errno));
				break;
			}
		}
		break;

	default:
		eerror("%s: caught unknown signal %d", applet, sig);
	}

	/* Restore errno */
	errno = serrno;
}

static char *
expand_home(const char *home, const char *path)
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

int main(int argc, char **argv)
{
	int devnull_fd = -1;
#ifdef TIOCNOTTY
	int tty_fd = -1;
#endif

#ifdef HAVE_PAM
	pam_handle_t *pamh = NULL;
	int pamr;
	const char *const *pamenv = NULL;
#endif

	int opt;
	size_t size = 0;
	bool start = false;
	bool stop = false;
	bool oknodo = false;
	bool test = false;
	char *exec = NULL;
	char *startas = NULL;
	char *name = NULL;
	char *pidfile = NULL;
	char *retry = NULL;
	int sig = -1;
	int nicelevel = INT_MIN, ionicec = -1, ioniced = 0;
	int oom_score_adj = INT_MIN;
	bool background = false;
	bool makepidfile = false;
	bool interpreted = false;
	bool progress = false;
	uid_t uid = 0;
	gid_t gid = 0;
	char *home = NULL;
	int tid = 0;
	char *redirect_stdin = NULL;
	char *redirect_stderr = NULL;
	char *redirect_stdout = NULL;
	char *stderr_process = NULL;
	char *stdout_process = NULL;
	int stdin_fd;
	int stdout_fd;
	int stderr_fd;
	pid_t pid, spid;
	RC_PIDLIST *pids;
	int i;
	char *svcname = getenv("RC_SVCNAME");
	RC_STRINGLIST *env_list;
	RC_STRING *env;
	char *tmp, *newpath, *np;
	char *p;
	char *token;
	char *exec_file = NULL;
	struct passwd *pw;
	struct group *gr;
	char *line = NULL;
	FILE *fp;
	size_t len;
	mode_t numask = 022;
	char **margv;
	unsigned int start_wait = 0;
	const char *scheduler = NULL;
	int sched_prio = -1;
#ifdef __linux__
	cap_iab_t cap_iab = NULL;
	unsigned secbits = 0;
#endif
#ifdef PR_SET_NO_NEW_PRIVS
	bool no_new_privs = false;
#endif
	int pipefd[2];
	char readbuf[1];
	ssize_t ss;
	struct notify notify = {0};
	int ret = EXIT_SUCCESS;

	applet = basename_c(argv[0]);
	atexit(cleanup);

	signal_setup(SIGINT, handle_signal);
	signal_setup(SIGQUIT, handle_signal);
	signal_setup(SIGTERM, handle_signal);

	if (rc_yesno(getenv("RC_USER_SERVICES")))
		rc_set_user();

	openlog(applet, LOG_PID, LOG_DAEMON);

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
	if ((tmp = getenv("SSD_OOM_SCORE_ADJ")))
		if (sscanf(tmp, "%d", &oom_score_adj) != 1)
			eerror("%s: invalid oom_score_adj `%s' (SSD_OOM_SCORE_ADJ)",
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
		case LONGOPT_CAPABILITIES:
#ifdef __linux__
			cap_iab = cap_iab_from_text(optarg);
			if (cap_iab == NULL)
				eerrorx("Could not parse iab: %s", strerror(errno));
#else
			eerrorx("Capabilities support not enabled");
#endif
			break;

		case LONGOPT_SECBITS:
#ifdef __linux__
			if (*optarg == '\0')
				eerrorx("Secbits are empty");

			tmp = NULL;
			secbits = strtoul(optarg, &tmp, 0);
			if (*tmp != '\0')
				eerrorx("Could not parse secbits: invalid char %c", *tmp);
#else
			eerrorx("Capabilities support not enabled");
#endif
			break;

		case LONGOPT_NO_NEW_PRIVS:
#ifdef PR_SET_NO_NEW_PRIVS
			no_new_privs = true;
#else
			eerrorx("The No New Privs flag is only supported by Linux (since 3.5)");
#endif
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

		case LONGOPT_OOM_SCORE_ADJ: /* --oom-score-adj */
			if (sscanf(optarg, "%d", &oom_score_adj) != 1)
				eerrorx("%s: invalid oom-score-adj `%s'",
				    applet, optarg);
			break;

		case 'P':  /* --progress */
			progress = true;
			break;

		case 'R':  /* --retry <schedule>|<timeout> */
			retry = optarg;
			break;

		case 'S':  /* --start */
			start = true;
			break;

		case 'b':  /* --background */
			background = true;
			break;

		case 'c':  /* --chuid <username>|<uid> */
			/* DEPRECATED */
			ewarn("WARNING: -c/--chuid is deprecated and will be removed in the future, please use -u/--user instead");
			/* falls through */
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

		case 'i': /* --interpreted */
			interpreted = true;
			break;

		case 'k':
			if (parse_mode(&numask, optarg))
				eerrorx("%s: invalid mode `%s'",
				    applet, optarg);
			break;

		case 'm':  /* --make-pidfile */
			makepidfile = true;
			break;

		case 'n':  /* --name <process-name> */
			name = optarg;
			break;

		case 'o':  /* --oknodo */
			/* DEPRECATED */
			ewarn("WARNING: -o/--oknodo is deprecated and will be removed in the future");
			oknodo = true;
			break;

		case 'p':  /* --pidfile <pid-file> */
			pidfile = optarg;
			break;

		case 's':  /* --signal <signal> */
			sig = parse_signal(applet, optarg);
			break;

		case 't':  /* --test */
			test = true;
			break;

		case 'r':  /* --chroot /new/root */
			ch_root = optarg;
			break;

		case 'a': /* --startas <name> */
			/* DEPRECATED */
			ewarn("WARNING: -a/--startas is deprecated and will be removed in the future, please use -x/--exec or -n/--name instead");
			startas = optarg;
			break;
		case 'w':
			if (sscanf(optarg, "%u", &start_wait) != 1)
				eerrorx("%s: `%s' not a number",
				    applet, optarg);
			break;
		case 'x':  /* --exec <executable> */
			exec = optarg;
			break;

		case '0':   /* --stdin /path/to/stdin.input-file */
			redirect_stdin = optarg;
			break;

		case '1':   /* --stdout /path/to/stdout.lgfile */
			redirect_stdout = optarg;
			break;

		case '2':  /* --stderr /path/to/stderr.logfile */
			redirect_stderr = optarg;
			break;

		case '3':   /* --stdout-logger "command to run for stdout logging" */
			stdout_process = optarg;
			break;

		case '4':  /* --stderr-logger "command to run for stderr logging" */
			stderr_process = optarg;
			break;

		case LONGOPT_SCHEDULER: /* --scheduler "Process scheduler policy" */
			scheduler = optarg;
			break;

		case LONGOPT_SCHEDULER_PRIO: /* --scheduler-priority "Process scheduler priority" */
			sscanf(optarg, "%d", &sched_prio);
			break;

		case LONGOPT_NOTIFY:
			notify = notify_parse(svcname ? svcname : applet, optarg);
			break;

		case_RC_COMMON_GETOPT
		}

	endpwent();
	argc -= optind;
	argv += optind;

	/* Allow start-stop-daemon --signal HUP --exec /usr/sbin/dnsmasq
	 * instead of forcing --stop --oknodo as well */
	if (!start &&
	    !stop &&
	    sig != SIGINT &&
	    sig != SIGTERM &&
	    sig != SIGQUIT &&
	    sig != SIGKILL)
		oknodo = true;

	if (!exec)
		exec = startas;
	else if (!name)
		name = startas;

	if (!exec) {
		exec = *argv;
		if (!exec)
			exec = name;
		if (name && start)
			*argv = name;
	} else if (name) {
		*--argv = name;
		++argc;
	} else if (exec) {
		*--argv = exec;
		++argc;
	};

	if (stop || sig != -1) {
		if (sig == -1)
			sig = SIGTERM;
		if (!*argv && !pidfile && !name && !uid)
			eerrorx("%s: --stop needs --exec, --pidfile,"
			    " --name or --user", applet);
		if (background)
			eerrorx("%s: --background is only relevant with"
			    " --start", applet);
		if (makepidfile)
			eerrorx("%s: --make-pidfile is only relevant with"
			    " --start", applet);
		if (redirect_stdout || redirect_stderr)
			eerrorx("%s: --stdout and --stderr are only relevant"
			    " with --start", applet);
		if (stdout_process || stderr_process)
			eerrorx("%s: --stdout-logger and --stderr-logger are only relevant"
			    " with --start", applet);
		if (start_wait)
			ewarn("using --wait with --stop has no effect,"
			    " use --retry instead");
	} else {
		if (!exec)
			eerrorx("%s: nothing to start", applet);
		if (makepidfile && !pidfile)
			eerrorx("%s: --make-pidfile is only relevant with"
			    " --pidfile", applet);
		if ((redirect_stdout || redirect_stderr) && !background)
			eerrorx("%s: --stdout and --stderr are only relevant"
			    " with --background", applet);
		if ((stdout_process || stderr_process) && !background)
			eerrorx("%s: --stdout-logger and --stderr-logger are only relevant"
			    " with --background", applet);
		if (redirect_stdout && stdout_process)
			eerrorx("%s: do not use --stdout and --stdout-logger together",
					applet);
		if (redirect_stderr && stderr_process)
			eerrorx("%s: do not use --stderr and --stderr-logger together",
					applet);
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
	}
	if (start && !exists(exec_file)) {
		eerror("%s: %s does not exist", applet,
		    exec_file ? exec_file : exec);
		free(exec_file);
		exit(EXIT_FAILURE);
	}
	if (start && retry)
		ewarn("using --retry with --start has no effect,"
		    " use --wait instead");

	/* If we don't have a pidfile we should check if it's interpreted
	 * or not. If it we, we need to pass the interpreter through
	 * to our daemon calls to find it correctly. */
	if (interpreted && !pidfile) {
		fp = fopen(exec_file, "r");
		if (fp) {
			line = NULL;
			if (xgetline(&line, &size, fp) == -1)
				eerrorx("%s: %s", applet, strerror(errno));
			p = line;
			fclose(fp);
			if (p != NULL && line[0] == '#' && line[1] == '!') {
				p = line + 2;
				/* Strip leading spaces */
				while (*p == ' ' || *p == '\t')
					p++;
				token = strsep(&p, " ");
				free(exec_file);
				xasprintf(&exec_file, "%s", token);
				opt = 0;
				for (nav = argv; *nav; nav++)
					opt++;
				nav = xmalloc(sizeof(char *) * (opt + 3));
				nav[0] = exec_file;
				len = 1;
				if (p)
					nav[len++] = p;
				for (i = 0; i < opt; i++)
					nav[i + len] = argv[i];
				nav[i + len] = NULL;
			}
		}
	}
	margv = nav ? nav : argv;

	if (stop || sig != -1) {
		if (sig == -1)
			sig = SIGTERM;
		if (!stop)
			oknodo = true;
		if (retry)
			parse_schedule(applet, retry, sig);
		else if (test || oknodo)
			parse_schedule(applet, "0", sig);
		else
			parse_schedule(applet, NULL, sig);
		if (pidfile) {
			pid = get_pid(applet, pidfile);
			if (pid == -1 && errno != ENOENT)
				exit(EXIT_FAILURE);
		} else {
			pid = 0;
		}
		i = run_stop_schedule(applet, exec, (const char *const *)margv,
		    pid, uid, test, progress, false);

		if (i < 0)
			/* We failed to stop something */
			exit(EXIT_FAILURE);
		if (test || oknodo)
			return i > 0 ? EXIT_SUCCESS : EXIT_FAILURE;

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

	if (pidfile)
		pid = get_pid(applet, pidfile);
	else
		pid = 0;

	if (pid)
		pids = rc_find_pids(NULL, NULL, 0, pid);
	else
		pids = rc_find_pids(exec, (const char * const *) argv, uid, 0);
	if (pids)
		eerrorx("%s: %s is already running", applet, exec);

	free(pids);
	if (test) {
		if (rc_yesno(getenv("EINFO_QUIET")))
			exit (EXIT_SUCCESS);

		einfon("Would start");
		while (argc-- > 0)
			printf(" %s", *argv++);
		printf("\n");
		eindent();
		if (uid != 0)
			einfo("as user id %d", uid);
		if (gid != 0)
			einfo("as group id %d", gid);
		if (ch_root)
			einfo("in root `%s'", ch_root);
		if (ch_dir)
			einfo("in dir `%s'", ch_dir);
		if (nicelevel != 0)
			einfo("with a priority of %d", nicelevel);
		if (name)
			einfo ("with a process name of %s", name);
		eoutdent();
		exit(EXIT_SUCCESS);
	}

	ebeginv("Detaching to start `%s'", exec);
	eindentv();

	/* Remove existing pidfile */
	if (pidfile)
		unlink(pidfile);

	if (background)
		signal_setup(SIGCHLD, handle_signal);

	/* Use a pipe to sync the parent/child processes. */
	if (pipe2(pipefd, O_CLOEXEC) == -1)
		eerrorx("%s: pipe2: %s", applet, strerror(errno));

	if ((pid = fork()) == -1)
		eerrorx("%s: fork: %s", applet, strerror(errno));

	/* Child process - lets go! */
	if (pid == 0) {
		pid_t mypid = getpid();
		close(pipefd[0]); /* Close the read end of the pipe. */
		umask(numask);

#ifdef TIOCNOTTY
		tty_fd = open("/dev/tty", O_RDWR);
#endif

		devnull_fd = open("/dev/null", O_RDWR);

		/* Must call setsid() before setting autogroup nicelevel
		 * but after opening tty_fd. */
		setsid();

		if (nicelevel != INT_MIN) {
			if (setpriority(PRIO_PROCESS, mypid, nicelevel) == -1)
				eerrorx("%s: setpriority %d: %s",
				    applet, nicelevel,
				    strerror(errno));
			/* Open in "r+" mode to avoid creating if non-existent. */
			fp = fopen("/proc/self/autogroup", "r+");
			if (fp) {
				fprintf(fp, "%d\n", nicelevel);
				fclose(fp);
			} else if (errno != ENOENT)
				eerrorx("%s: autogroup nice %d: %s", applet,
				    nicelevel, strerror(errno));
		}

		if (ionicec != -1 &&
		    ioprio_set(1, mypid, ionicec | ioniced) == -1)
			eerrorx("%s: ioprio_set %d %d: %s", applet,
			    ionicec, ioniced, strerror(errno));

		if (oom_score_adj != INT_MIN) {
			fp = fopen("/proc/self/oom_score_adj", "w");
			if (!fp)
				eerrorx("%s: oom_score_adj %d: %s", applet,
				    oom_score_adj, strerror(errno));
			fprintf(fp, "%d\n", oom_score_adj);
			fclose(fp);
		}

		if (ch_root && chroot(ch_root) < 0)
			eerrorx("%s: chroot `%s': %s",
			    applet, ch_root, strerror(errno));

		if (ch_dir && chdir(ch_dir) < 0)
			eerrorx("%s: chdir `%s': %s",
			    applet, ch_dir, strerror(errno));

		if (makepidfile && pidfile) {
			fp = fopen(pidfile, "w");
			if (!fp)
				eerrorx("%s: fopen `%s': %s", applet, pidfile,
				    strerror(errno));
			fprintf(fp, "%d\n", mypid);
			fclose(fp);
		}

#ifdef HAVE_PAM
		if (changeuser != NULL) {
			pamr = pam_start("start-stop-daemon",
			    changeuser, &conv, &pamh);

			if (pamr == PAM_SUCCESS)
				pamr = pam_acct_mgmt(pamh, PAM_SILENT);
			if (pamr == PAM_SUCCESS)
				pamr = pam_open_session(pamh, PAM_SILENT);
			if (pamr != PAM_SUCCESS)
				eerrorx("%s: pam error: %s",
					applet, pam_strerror(pamh, pamr));
		}
#endif

		if (gid && setgid(gid))
			eerrorx("%s: unable to set groupid to %d",
			    applet, gid);
		if (changeuser && initgroups(changeuser, gid))
			eerrorx("%s: initgroups (%s, %d)",
			    applet, changeuser, gid);
#ifdef __linux__
		if (uid && cap_setuid(uid))
#else
		if (uid && setuid(uid))
#endif
			eerrorx ("%s: unable to set userid to %d",
			    applet, uid);

		/* Close any fd's to the passwd database */
		endpwent();

#ifdef __linux__
		if (cap_iab != NULL) {
			i = cap_iab_set_proc(cap_iab);

			if (cap_free(cap_iab) != 0)
				eerrorx("Could not releasable memory: %s", strerror(errno));

			if (i != 0)
				eerrorx("Could not set iab: %s", strerror(errno));
		}

		if (secbits != 0) {
			if (cap_set_secbits(secbits) < 0)
				eerrorx("Could not set securebits to 0x%x: %s", secbits, strerror(errno));
		}
#endif

#ifdef PR_SET_NO_NEW_PRIVS
		if (no_new_privs) {
			if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0) == -1)
				eerrorx("Could not set No New Privs flag: %s", strerror(errno));
		}
#endif


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
			if (strncmp(env->value, "RC_", 3) == 0 && strncmp(env->value, "SSD_", 4) == 0) {
				*strchr(env->value, '=') = '\0';
				unsetenv(env->value);
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
		if (redirect_stdin) {
			if ((stdin_fd = open(redirect_stdin,
				    O_RDONLY)) == -1)
				eerrorx("%s: unable to open the input file"
				    " for stdin `%s': %s",
				    applet, redirect_stdin, strerror(errno));
		}
		if (redirect_stdout) {
			if ((stdout_fd = open(redirect_stdout,
				    O_WRONLY | O_CREAT | O_APPEND,
				    S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP)) == -1)
				eerrorx("%s: unable to open the logfile"
				    " for stdout `%s': %s",
				    applet, redirect_stdout, strerror(errno));
		}else if (stdout_process) {
			stdout_fd = rc_pipe_command(stdout_process, devnull_fd);
			if (stdout_fd == -1)
				eerrorx("%s: unable to open the logging process"
				    " for stdout `%s': %s",
				    applet, stdout_process, strerror(errno));
		}
		if (redirect_stderr) {
			if ((stderr_fd = open(redirect_stderr,
				    O_WRONLY | O_CREAT | O_APPEND,
				    S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP)) == -1)
				eerrorx("%s: unable to open the logfile"
				    " for stderr `%s': %s",
				    applet, redirect_stderr, strerror(errno));
		}else if (stderr_process) {
			stderr_fd = rc_pipe_command(stderr_process, devnull_fd);
			if (stderr_fd == -1)
				eerrorx("%s: unable to open the logging process"
				    " for stderr `%s': %s",
				    applet, stderr_process, strerror(errno));
		}

		if (background)
			dup2(stdin_fd, STDIN_FILENO);
		if (background || redirect_stdout || stdout_process
				|| rc_yesno(getenv("EINFO_QUIET")))
			dup2(stdout_fd, STDOUT_FILENO);
		if (background || redirect_stderr || stderr_process
				|| rc_yesno(getenv("EINFO_QUIET")))
			dup2(stderr_fd, STDERR_FILENO);

		cloexec_fds_from(3);

		if (notify.type == NOTIFY_FD) {
			if (close(notify.pipe[0]) == -1)
				eerrorx("%s: failed to close notify pipe[0]: %s", applet, strerror(errno));
			if (dup2(notify.pipe[1], notify.fd) == -1)
				eerrorx("%s: failed to initialize notify fd: %s", applet, strerror(errno));
		}

		if (scheduler != NULL) {
			int scheduler_index;
			struct sched_param sched =  {.sched_priority = sched_prio};
			if (strcmp(scheduler, "fifo") == 0)
				scheduler_index = SCHED_FIFO;
			else if (strcmp(scheduler, "rr") == 0)
				scheduler_index = SCHED_RR;
			else if (strcmp(scheduler, "other") == 0)
				scheduler_index = SCHED_OTHER;
#ifdef SCHED_BATCH
			else if (strcmp(scheduler, "batch") == 0)
				scheduler_index = SCHED_BATCH;
#endif
#ifdef SCHED_IDLE
			else if (strcmp(scheduler, "idle") == 0)
				scheduler_index = SCHED_IDLE;
#endif
			else if (sscanf(scheduler, "%d", &scheduler_index) != 1)
				eerrorx("Unknown scheduler: %s", scheduler);

			if (sched_prio == -1)
				sched.sched_priority = sched_get_priority_min(scheduler_index);

			if (pthread_setschedparam(pthread_self(), scheduler_index, &sched))
				eerrorx("Failed to set scheduler: %s", strerror(errno));
		} else if (sched_prio != -1) {
			const struct sched_param sched =  {.sched_priority = sched_prio};
			if (sched_setparam(mypid, &sched))
				eerrorx("Failed to set scheduler parameters: %s", strerror(errno));
		}

		execvp(exec, argv);
#ifdef HAVE_PAM
		if (changeuser != NULL && pamr == PAM_SUCCESS)
			pam_close_session(pamh, PAM_SILENT);
#endif
		eerrorx("%s: failed to exec `%s': %s",
		    applet, exec,strerror(errno));
	}

	/* Parent process */

	close(pipefd[1]); /* Close the write end of the pipe. */

	/* The child never writes to the pipe, so this read will block until
	 * the child calls exec or exits. */
	while ((ss = read(pipefd[0], readbuf, 1)) == -1 && errno == EINTR);
	if (ss == -1)
		eerrorx("%s: failed to read from pipe: %s",
			applet, strerror(errno));

	close(pipefd[0]);

	if (!background) {
		/* As we're not backgrounding the process, wait for our pid
		 * to return */
		i = 0;
		spid = pid;

		do {
			pid = waitpid(spid, &i, 0);
			if (pid < 1) {
				eerror("waitpid %d: %s",
				    spid, strerror(errno));
				return -1;
			}
		} while (!WIFEXITED(i) && !WIFSIGNALED(i));
		if (!WIFEXITED(i) || WEXITSTATUS(i) != 0) {
			eerror("%s: failed to start `%s'", applet, exec);
			exit(EXIT_FAILURE);
		}
		pid = spid;
	}

	/* Wait a little bit and check that process is still running
	   We do this as some badly written daemons fork and then barf */
	if (start_wait == 0 &&
	    ((p = getenv("SSD_STARTWAIT")) ||
		(p = rc_conf_value("rc_start_wait"))))
	{
		if (sscanf(p, "%u", &start_wait) != 1)
			start_wait = 0;
	}

	if (notify.type != NOTIFY_NONE) {
		if (!notify_wait(applet, notify))
			ret = EXIT_FAILURE;
	} else if (start_wait > 0) {
		struct timespec ts;
		bool alive = false;

		ts.tv_sec = start_wait / 1000;
		ts.tv_nsec = (start_wait % 1000) * ONE_MS;
		if (nanosleep(&ts, NULL) == -1) {
			if (errno != EINTR) {
				eerror("%s: nanosleep: %s",
				    applet, strerror(errno));
				return 0;
			}
		}
		if (background) {
			if (kill(pid, 0) == 0)
				alive = true;
		} else {
			if (pidfile) {
				pid = get_pid(applet, pidfile);
				if (pid == -1) {
					eerrorx("%s: did not "
					    "create a valid"
					    " pid in `%s'",
					    applet, pidfile);
				}
			} else
				pid = 0;
			if (do_stop(applet, exec, (const char *const *)margv,
				pid, uid, 0, test, false) > 0)
				alive = true;
		}

		if (!alive)
			eerrorx("%s: %s died", applet, exec);
	}

	if (svcname)
		rc_service_daemon_set(svcname, exec,
		    (const char *const *)margv, pidfile, true);

	exit(ret);
	/* NOTREACHED */
}
