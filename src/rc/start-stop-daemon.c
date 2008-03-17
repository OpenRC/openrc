/*
   start-stop-daemon
   Starts, stops, tests and signals daemons

   This is essentially a ground up re-write of Debians
   start-stop-daemon for cleaner code and to integrate into our RC
   system so we can monitor daemons a little.
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

/* nano seconds */
#define POLL_INTERVAL   20000000
#define WAIT_PIDFILE   500000000
#define START_WAIT     100000000
#define ONE_SECOND    1000000000

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/termios.h>
#include <sys/time.h>
#include <sys/wait.h>

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <grp.h>
#include <pwd.h>
#include <signal.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#ifdef HAVE_PAM
#include <security/pam_appl.h>

/* We are not supporting authentication conversations */
static struct pam_conv conv = { NULL, NULL};
#endif

#include "builtins.h"
#include "einfo.h"
#include "rc.h"
#include "rc-misc.h"

/* Some libc implementations don't define this */
#ifndef LIST_FOREACH_SAFE
#define	LIST_FOREACH_SAFE(var, head, field, tvar)			\
	for ((var) = LIST_FIRST((head));				\
	     (var) && ((tvar) = LIST_NEXT((var), field), 1);		\
	     (var) = (tvar))
#endif


typedef struct scheduleitem
{
	enum
	{
		SC_TIMEOUT,
		SC_SIGNAL,
		SC_GOTO,
		SC_FOREVER
	} type;
	int value;
	struct scheduleitem *gotoitem;
	STAILQ_ENTRY(scheduleitem) entries;
} SCHEDULEITEM;
STAILQ_HEAD(, scheduleitem) schedule;

extern const char *applet;
static char *changeuser;

extern char **environ;

static void free_schedulelist(void)
{
	SCHEDULEITEM *s1 = STAILQ_FIRST(&schedule);
	SCHEDULEITEM *s2;

	while (s1) {
		s2 = STAILQ_NEXT(s1, entries);
		free(s1);
		s1 = s2;
	}
	STAILQ_INIT(&schedule);
}

static void cleanup(void)
{
	if (changeuser)
		free(changeuser);

	free_schedulelist();
}

static int parse_signal(const char *sig)
{
	typedef struct signalpair
	{
		const char *name;
		int signal;
	} SIGNALPAIR;

	static const SIGNALPAIR signallist[] = {
		{ "ABRT",	SIGABRT	},
		{ "ALRM",	SIGALRM	},
		{ "FPE",	SIGFPE	},
		{ "HUP",	SIGHUP	},
		{ "ILL",	SIGILL	},
		{ "INT",	SIGINT	},
		{ "KILL",	SIGKILL	},
		{ "PIPE",	SIGPIPE	},
		{ "QUIT",	SIGQUIT	},
		{ "SEGV",	SIGSEGV	},
		{ "TERM",	SIGTERM	},
		{ "USR1",	SIGUSR1	},
		{ "USR2",	SIGUSR2	},
		{ "CHLD",	SIGCHLD	},
		{ "CONT",	SIGCONT	},
		{ "STOP",	SIGSTOP	},
		{ "TSTP",	SIGTSTP	},
		{ "TTIN",	SIGTTIN	},
		{ "TTOU",	SIGTTOU	}
	};

	unsigned int i = 0;
	const char *s;

	if (! sig || *sig == '\0')
		return -1;

	if (sscanf(sig, "%u", &i) == 1) {
		if (i > 0 && i < sizeof(signallist) / sizeof(signallist[0]))
			return i;
		eerrorx("%s: `%s' is not a valid signal", applet, sig);
	}

	if (strncmp(sig, "SIG", 3) == 0)
		s = sig + 3;
	else
		s = NULL;

	for (i = 0; i < sizeof(signallist) / sizeof(signallist[0]); i++)
		if (strcmp(sig, signallist[i].name) == 0 ||
		    (s && strcmp(s, signallist[i].name) == 0))
			return signallist[i].signal;

	eerrorx("%s: `%s' is not a valid signal", applet, sig);
	/* NOTREACHED */
}

static SCHEDULEITEM *parse_schedule_item(const char *string)
{
	const char *after_hyph;
	int sig;
	SCHEDULEITEM *item = xmalloc(sizeof(*item));

	item->value = 0;
	item->gotoitem = NULL;
	if (strcmp(string,"forever") == 0)
		item->type = SC_FOREVER;
	else if (isdigit((int) string[0])) {
		item->type = SC_TIMEOUT;
		errno = 0;
		if (sscanf(string, "%d", &item->value) != 1)
			eerrorx("%s: invalid timeout value in schedule `%s'", applet,
				string);
	} else if ((after_hyph = string + (string[0] == '-')) &&
		   ((sig = parse_signal(after_hyph)) != -1))
	{
		item->type = SC_SIGNAL;
		item->value = (int)sig;
	}
	else
		eerrorx("%s: invalid schedule item `%s'", applet, string);

	return item;
}

static void parse_schedule(const char *string, int timeout)
{
	char buffer[20];
	const char *slash;
	int count = 0;
	SCHEDULEITEM *repeatat = NULL;
	size_t len;
	SCHEDULEITEM *item;

	if (string)
		for (slash = string; *slash; slash++)
			if (*slash == '/')
				count++;

	free_schedulelist();

	if (count == 0) {
		item = xmalloc(sizeof(*item));
		item->type = SC_SIGNAL;
		item->value = timeout;
		item->gotoitem = NULL;
		STAILQ_INSERT_TAIL(&schedule, item, entries);

		item = xmalloc(sizeof(*item));
		item->type = SC_TIMEOUT;
		item->gotoitem = NULL;
		STAILQ_INSERT_TAIL(&schedule, item, entries);
		if (string) {
			if (sscanf(string, "%d", &item->value) != 1)
				eerrorx("%s: invalid timeout value in schedule", applet);
		} else
			item->value = 5;

		return;
	}

	while (string != NULL) {
		if ((slash = strchr(string, '/')))
			len = slash - string;
		else
			len = strlen(string);

		if (len >= (ptrdiff_t) sizeof(buffer))
			eerrorx("%s: invalid schedule item, far too long", applet);

		memcpy(buffer, string, len);
		buffer[len] = 0;
		string = slash ? slash + 1 : NULL;

		item = parse_schedule_item(buffer);
		STAILQ_INSERT_TAIL(&schedule, item, entries);
		if (item->type == SC_FOREVER) {
			if (repeatat)
				eerrorx("%s: invalid schedule, `forever' "
					"appears more than once", applet);

			repeatat = item;
			continue;
		}
	}

	if (repeatat) {
		item = xmalloc(sizeof(*item));
		item->type = SC_GOTO;
		item->value = 0;
		item->gotoitem = repeatat;
		STAILQ_INSERT_TAIL(&schedule, item, entries);
	}

	return;
}

static pid_t get_pid(const char *pidfile, bool quiet)
{
	FILE *fp;
	pid_t pid;

	if (! pidfile)
		return -1;

	if ((fp = fopen(pidfile, "r")) == NULL) {
		if (! quiet)
			eerror("%s: fopen `%s': %s", applet, pidfile, strerror(errno));
		return -1;
	}

	if (fscanf(fp, "%d", &pid) != 1) {
		if (! quiet)
			eerror("%s: no pid found in `%s'", applet, pidfile);
		fclose(fp);
		return -1;
	}
	
	fclose(fp);

	return pid;
}

/* return number of processed killed, -1 on error */
static int do_stop(const char *const *argv, const char *cmd,
		   const char *pidfile, uid_t uid,int sig,
		   bool quiet, bool verbose, bool test)
{
	RC_PIDLIST *pids;
	RC_PID *pi;
	RC_PID *np;
	bool killed;
	int nkilled = 0;
	pid_t pid = 0;

	if (pidfile) {
		if ((pid = get_pid(pidfile, quiet)) == -1)
			return quiet ? 0 : -1;
		pids = rc_find_pids(NULL, NULL, 0, pid);
	} else
		pids = rc_find_pids(argv, cmd, uid, pid);

	if (! pids)
		return 0;

	LIST_FOREACH_SAFE(pi, pids, entries, np) {
		if (test) {
			if (! quiet)
				einfo("Would send signal %d to PID %d",
				      sig, pi->pid);
			nkilled++;
		} else {
			if (verbose)
				ebegin("Sending signal %d to PID %d",
				sig, pi->pid);
			errno = 0;
			killed = (kill(pi->pid, sig) == 0 ||
					errno == ESRCH ? true : false);
			if (verbose)
				eend(killed ? 0 : 1,
				     "%s: failed to send signal %d to PID %d: %s",
				     applet, sig, pi->pid, strerror(errno));
			if (! killed) {
				nkilled = -1;
			} else {
				if (nkilled != -1)
					nkilled++;
			}
		}
		free(pi);
	}

	free(pids);
	return nkilled;
}

static int run_stop_schedule(const char *const *argv, const char *cmd,
			     const char *pidfile, uid_t uid,
			     bool quiet, bool verbose, bool test)
{
	SCHEDULEITEM *item = STAILQ_FIRST(&schedule);
	int nkilled = 0;
	int tkilled = 0;
	int nrunning = 0;
	long nloops;
	struct timespec ts;

	if (verbose) {
		if (pidfile)
			einfo("Will stop PID in pidfile `%s'", pidfile);
		if (uid)
			einfo("Will stop processes owned by UID %d", uid);
		if (argv && *argv)
			einfo("Will stop processes of `%s'", *argv);
		if (cmd)
			einfo("Will stop processes called `%s'", cmd);
	}

	while (item) {
		switch (item->type) {
		case SC_GOTO:
			item = item->gotoitem;
			continue;

		case SC_SIGNAL:
			nrunning = 0;
			nkilled = do_stop(argv, cmd, pidfile, uid, item->value,
					  quiet, verbose, test);
			if (nkilled == 0) {
				if (tkilled == 0) {
					if (! quiet)
						eerror("%s: no matching "
						       "processes found", applet);
				}
				return tkilled;
			}
			else if (nkilled == -1)
				return 0;

			tkilled += nkilled;
			break;
		case SC_TIMEOUT:
			if (item->value < 1) {
				item = NULL;
				break;
			}

			nloops = (ONE_SECOND / POLL_INTERVAL) * item->value;
			ts.tv_sec = 0;
			ts.tv_nsec = POLL_INTERVAL;

			while (nloops) {
				if ((nrunning = do_stop(argv, cmd, pidfile,
							uid, 0, true, false, true)) == 0)
					return true;

				if (nanosleep(&ts, NULL) == -1) {
					if (errno == EINTR)
						eerror("%s: caught an interrupt", applet);
					else {
						eerror("%s: nanosleep: %s",
						       applet, strerror(errno));
						return 0;
					}
				}
					nloops --;
			}
			break;

		default:
			eerror("%s: invalid schedule item `%d'", applet, item->type);
			return 0;
		}

		if (item)
			item = STAILQ_NEXT(item, entries);
	}

	if (test || (tkilled > 0 && nrunning == 0))
		return nkilled;

	if (! quiet) {
		if (nrunning == 1)
			eerror("%s: %d process refused to stop", applet, nrunning);
		else
			eerror("%s: %d process(es) refused to stop", applet, nrunning);
	}

	return -nrunning;
}

static void handle_signal(int sig)
{
	int status;
	int serrno = errno;
	char signame[10] = { '\0' };

	switch (sig) {
	case SIGINT:
		if (! signame[0])
			snprintf(signame, sizeof(signame), "SIGINT");
		/* FALLTHROUGH */
	case SIGTERM:
		if (! signame[0])
			snprintf(signame, sizeof(signame), "SIGTERM");
		/* FALLTHROUGH */
	case SIGQUIT:
		if (! signame[0])
			snprintf(signame, sizeof(signame), "SIGQUIT");
		eerrorx("%s: caught %s, aborting", applet, signame);
		/* NOTREACHED */

	case SIGCHLD:
		for (;;) {
			if (waitpid(-1, &status, WNOHANG) < 0) {
				if (errno != ECHILD)
					eerror("%s: waitpid: %s", applet, strerror(errno));
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


#include "_usage.h"
#define getoptstring "KN:R:Sbc:d:g:mn:op:s:tu:r:x:1:2:" getoptstring_COMMON
static const struct option longopts[] = {
	{ "stop",         0, NULL, 'K'},
	{ "nicelevel",    1, NULL, 'N'},
	{ "retry",        1, NULL, 'R'},
	{ "start",        0, NULL, 'S'},
	{ "startas",      1, NULL, 'a'},
	{ "background",   0, NULL, 'b'},
	{ "chuid",        1, NULL, 'c'},
	{ "chdir",        1, NULL, 'd'},
	{ "env",          1, NULL, 'e'},
	{ "group",        1, NULL, 'g'},
	{ "make-pidfile", 0, NULL, 'm'},
	{ "name",         1, NULL, 'n'},
	{ "oknodo",       0, NULL, 'o'},
	{ "pidfile",      1, NULL, 'p'},
	{ "signal",       1, NULL, 's'},
	{ "test",         0, NULL, 't'},
	{ "user",         1, NULL, 'u'},
	{ "chroot",       1, NULL, 'r'},
	{ "exec",         1, NULL, 'x'},
	{ "stdout",       1, NULL, '1'},
	{ "stderr",       1, NULL, '2'},
	longopts_COMMON
};
static const char * const longopts_help[] = {
	"Stop daemon",
	"Set a nicelevel when starting",
	"Retry schedule to use when stopping",
	"Start daemon",
	"deprecated, use --exec",
	"Force daemon to background",
	"deprecated, use --user",
	"Change the PWD",
	"Set an environment string",
	"Change the process group",
	"Create a pidfile",
	"Match process name",
	"deprecated",
	"Match pid found in this file",
	"Send a different signal",
	"Test actions, don't do them",
	"Change the process user",
	"Chroot to this directory",
	"Binary to start/stop",
	"Redirect stdout to file",
	"Redirect stderr to file",
	longopts_help_COMMON
};
#include "_usage.c"

int start_stop_daemon(int argc, char **argv)
{
	int devnull_fd = -1;
#ifdef TIOCNOTTY
	int tty_fd = -1;
#endif

#ifdef HAVE_PAM
	pam_handle_t *pamh = NULL;
	int pamr;
#endif

	int opt;
	bool start = false;
	bool stop = false;
	bool oknodo = false;
	bool test = false;
	bool quiet;
	bool verbose = false;
	char *exec = NULL;
	char *cmd = NULL;
	char *pidfile = NULL;
	int sig = SIGTERM;
	int nicelevel = 0;
	bool background = false;
	bool makepidfile = false;
	uid_t uid = 0;
	gid_t gid = 0;
	char *ch_root = NULL;
	char *ch_dir = NULL;
	int tid = 0;
	char *redirect_stderr = NULL;
	char *redirect_stdout = NULL;
	int stdout_fd;
	int stderr_fd;
	pid_t pid;
	int i;
	char *svcname = getenv("SVCNAME");
	RC_STRINGLIST *env_list;
	RC_STRING *env;
	char *path;
	bool sethome = false;
	bool setuser = false;
	char *p;
	char *tmp;
	struct passwd *pw;
	struct group *gr;
	char line[130];
	FILE *fp;
	size_t len;

	STAILQ_INIT(&schedule);
	atexit(cleanup);

	signal_setup(SIGINT, handle_signal);
	signal_setup(SIGQUIT, handle_signal);
	signal_setup(SIGTERM, handle_signal);

	if ((path = getenv("SSD_NICELEVEL")))
		if (sscanf(path, "%d", &nicelevel) != 1)
			eerror("%s: invalid nice level `%s' (SSD_NICELEVEL)",
				applet, path);

	while ((opt = getopt_long(argc, argv, "e:" getoptstring, longopts,
				  (int *) 0)) != -1)
		switch (opt) {
		case 'K':  /* --stop */
			stop = true;
			break;

		case 'N':  /* --nice */
			if (sscanf(optarg, "%d", &nicelevel) != 1)
				eerrorx("%s: invalid nice level `%s'", applet, optarg);
			break;

		case 'R':  /* --retry <schedule>|<timeout> */
			parse_schedule(optarg, sig);
			break;

		case 'S':  /* --start */
			start = true;
			break;

		case 'b':  /* --background */
			background = true;
			break;

		case 'u':  /* --user <username>|<uid> */
		case 'c':  /* --chuid <username>|<uid> */
			{
				p = optarg;
				tmp = strsep(&p, ":");
				changeuser = xstrdup(tmp);
				if (sscanf(tmp, "%d", &tid) != 1)
					pw = getpwnam(tmp);
				else
					pw = getpwuid((uid_t) tid);

				if (! pw)
					eerrorx("%s: user `%s' not found", applet, tmp);
				uid = pw->pw_uid;
				if (! gid)
					gid = pw->pw_gid;

				if (p) {
					tmp = strsep (&p, ":");
					if (sscanf(tmp, "%d", &tid) != 1)
						gr = getgrnam(tmp);
					else
						gr = getgrgid((gid_t) tid);

					if (! gr)
						eerrorx("%s: group `%s' not found",
							applet, tmp);
					gid = gr->gr_gid;
				}
			}
			break;

		case 'd':  /* --chdir /new/dir */
			ch_dir = optarg;
			break;

		case 'e': /* --env */
			if (putenv(optarg) == 0) {
				if (strncmp("HOME=", optarg, 5) == 0)
					sethome = true;
				else if (strncmp("USER=", optarg, 5) == 0)
					setuser = true;
			}
			break;

		case 'g':  /* --group <group>|<gid> */
			{
				if (sscanf(optarg, "%d", &tid) != 1)
					gr = getgrnam(optarg);
				else
					gr = getgrgid((gid_t) tid);

				if (! gr)
					eerrorx("%s: group `%s' not found", applet, optarg);
				gid = gr->gr_gid;
			}
			break;

		case 'm':  /* --make-pidfile */
			makepidfile = true;
			break;

		case 'n':  /* --name <process-name> */
			cmd = optarg;
			break;

		case 'o':  /* --oknodo */
			oknodo = true;
			break;

		case 'p':  /* --pidfile <pid-file> */
			pidfile = optarg;
			break;

		case 's':  /* --signal <signal> */
			sig = parse_signal(optarg);
			break;

		case 't':  /* --test */
			test = true;
			break;

		case 'r':  /* --chroot /new/root */
			ch_root = optarg;
			break;

		case 'a':
		case 'x':  /* --exec <executable> */
			exec = optarg;
			break;

		case '1':   /* --stdout /path/to/stdout.lgfile */
			redirect_stdout = optarg;
			break;

		case '2':  /* --stderr /path/to/stderr.logfile */
			redirect_stderr = optarg;
			break;

			case_RC_COMMON_GETOPT
		}

	quiet = rc_yesno(getenv("EINFO_QUIET"));
	verbose = rc_yesno(getenv("EINFO_VERBOSE"));

	/* Allow start-stop-daemon --signal HUP --exec /usr/sbin/dnsmasq
	 * instead of forcing --stop --oknodo as well */
	if (! start && ! stop)
		if (sig != SIGINT &&
		    sig != SIGTERM &&
		    sig != SIGQUIT &&
		    sig != SIGKILL)
		{
			oknodo = true;
			stop = true;
		}

	if (start == stop)
		eerrorx("%s: need one of --start or --stop", applet);

	if (start && ! exec)
		eerrorx("%s: --start needs --exec", applet);

	if (stop && ! exec && ! pidfile && ! cmd && ! uid)
		eerrorx("%s: --stop needs --exec, --pidfile, --name or --user", applet);

	if (makepidfile && ! pidfile)
		eerrorx("%s: --make-pidfile is only relevant with --pidfile", applet);

	if (background && ! start)
		eerrorx("%s: --background is only relevant with --start", applet);

	if ((redirect_stdout || redirect_stderr) && ! background)
		eerrorx("%s: --stdout and --stderr are only relevant with --background",
			 applet);

	argc -= optind;
	argv += optind;

	/* Validate that the binary exists if we are starting */
	if (exec) {
		if (ch_root)
			tmp = rc_strcatpaths(ch_root, exec, (char *) NULL);
		else
			tmp = exec;
		if (start && ! exists(tmp)) {
			eerror("%s: %s does not exist", applet, tmp);
			if (ch_root)
				free(tmp);
			exit(EXIT_FAILURE);
		}

		/* If we don't have a pidfile or name, check it's not
		 * interpreted, otherwise we should fail */
		if (! pidfile && ! cmd) {
			fp = fopen (tmp, "r");
			if (fp) {
				fgets(line, sizeof(line), fp);
				fclose(fp);

				if (line[0] == '#' && line[1] == '!') {
					len = strlen (line) - 1;

					/* Remove the trailing newline */
					if (line[len] == '\n')
						line[len] = '\0';

					eerror("%s: %s is a script",
						applet, exec);
					eerror("%s: and should be started, stopped"
					       " or signalled with ", applet);
					eerror("%s: --exec %s %s",
						applet, line + 2, exec);
					eerror("%s: or you should specify a pidfile"
					       " or process name", applet);
					if (ch_root)
						free(tmp);
					exit(EXIT_FAILURE);
				}
			}
		}

		if (ch_root)
			free(tmp);
	}

	/* Add exec to our arguments */
	*--argv = exec;

	if (stop) {
		int result;

		if (! STAILQ_FIRST(&schedule)) {
			if (test || oknodo)
				parse_schedule("0", sig);
			else
				parse_schedule(NULL, sig);
		}

		result = run_stop_schedule((const char *const *)argv, cmd,
					   pidfile, uid, quiet, verbose, test);

		if (result < 0)
			/* We failed to stop something */
			exit(EXIT_FAILURE);

		if (test || oknodo)
			return result > 0 ? EXIT_SUCCESS : EXIT_FAILURE;

		/* Even if we have not actually killed anything, we should
		 * remove information about it as it may have unexpectedly
		 * crashed out. We should also return success as the end
		 * result would be the same. */
		if (pidfile && exists(pidfile))
			unlink(pidfile);

		if (svcname)
			rc_service_daemon_set(svcname,
					      (const char *const *)argv,
					      cmd, pidfile, false);

		exit(EXIT_SUCCESS);
	}

	if (do_stop((const char * const *)argv, cmd, pidfile, uid,
		    0, true, false, true) > 0)
		eerrorx("%s: %s is already running", applet, exec);

	if (test) {
		if (quiet)
			exit (EXIT_SUCCESS);

		einfon("Would start");
		while (argc-- >= 0)
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
		eoutdent();
		exit(EXIT_SUCCESS);
	}

	if (verbose) {
		ebegin("Detaching to start `%s'", exec);
		eindent();
	}

	if (background)
		signal_setup(SIGCHLD, handle_signal);

	if ((pid = fork()) == -1)
		eerrorx("%s: fork: %s", applet, strerror(errno));

	/* Child process - lets go! */
	if (pid == 0) {
		pid_t mypid = getpid();

#ifdef TIOCNOTTY
		tty_fd = open("/dev/tty", O_RDWR);
#endif

		devnull_fd = open("/dev/null", O_RDWR);

		if (nicelevel) {
			if (setpriority(PRIO_PROCESS, mypid, nicelevel) == -1)
				eerrorx("%s: setpritory %d: %s", applet, nicelevel,
					 strerror(errno));
		}

		if (ch_root && chroot(ch_root) < 0)
			eerrorx("%s: chroot `%s': %s", applet, ch_root, strerror(errno));

		if (ch_dir && chdir (ch_dir) < 0)
			eerrorx("%s: chdir `%s': %s", applet, ch_dir, strerror(errno));

		if (makepidfile && pidfile) {
			fp = fopen(pidfile, "w");
			if (! fp)
				eerrorx("%s: fopen `%s': %s", applet, pidfile,
					strerror(errno));
			fprintf(fp, "%d\n", mypid);
			fclose(fp);
		}

#ifdef HAVE_PAM
		if (changeuser != NULL)
			pamr = pam_start("start-stop-daemon", changeuser, &conv, &pamh);
		else
			pamr = pam_start("start-stop-daemon", "nobody", &conv, &pamh);

		if (pamr == PAM_SUCCESS)
			pamr = pam_authenticate(pamh, PAM_SILENT);
		if (pamr == PAM_SUCCESS)
			pamr = pam_acct_mgmt(pamh, PAM_SILENT);
		if (pamr == PAM_SUCCESS)
			pamr = pam_open_session(pamh, PAM_SILENT);
		if (pamr != PAM_SUCCESS)
			eerrorx("%s: pam error: %s", applet, pam_strerror(pamh, pamr));
#endif

		if (gid && setgid(gid))
			eerrorx("%s: unable to set groupid to %d", applet, gid);
		if (changeuser && initgroups(changeuser, gid))
			eerrorx("%s: initgroups (%s, %d)", applet, changeuser, gid);
		if (uid && setuid(uid))
			eerrorx ("%s: unable to set userid to %d", applet, uid);
		else {
			pw = getpwuid(uid);
			if (pw) {
				if (! sethome) {
					unsetenv("HOME");
					if (pw->pw_dir)
						setenv("HOME", pw->pw_dir, 1);
				}
				if (! setuser) {
					unsetenv("USER");
					if (pw->pw_name)
						setenv("USER", pw->pw_name, 1);
				}
			}
		}

		/* Close any fd's to the passwd database */
		endpwent();

#ifdef TIOCNOTTY
		ioctl(tty_fd, TIOCNOTTY, 0);
		close(tty_fd);
#endif

		/* Clean the environment of any RC_ variables */
		env_list = rc_stringlist_new();
		i = 0;
		while(environ[i])
			rc_stringlist_add(env_list, environ[i++]);
		TAILQ_FOREACH(env, env_list, entries) {
			if ((strncmp(env->value, "RC_", 3) == 0 &&
			     strncmp(env->value, "RC_SERVICE=", strlen("RC_SERVICE=")) != 0) ||
			    strncmp(env->value, "SSD_NICELEVEL=", strlen("SSD_NICELEVEL=")) == 0)
			{
				p = strchr(env->value, '=');
				*p = '\0';
				unsetenv(env->value);
				continue;
			}
		}
		rc_stringlist_free(env_list);

		/* For the path, remove the rcscript bin dir from it */
		if ((path = getenv("PATH"))) {
			size_t mx = strlen(path);
			char *newpath = xmalloc(mx);
			char *token;
			char *np = newpath;
			size_t l;

			p = path;
			while ((token = strsep (&p, ":"))) {
				if (strcmp (token, RC_LIBDIR "/bin") == 0 ||
				    strcmp (token, RC_LIBDIR "/sbin") == 0)
					continue;

				l = strlen (token);
				if (np != newpath)
					*np++ = ':';
				memcpy (np, token, l);
				np += l;
				*np = '\0';
			}
			unsetenv("PATH");
			setenv("PATH", newpath, 1);
		}

		umask(022);

		stdout_fd = devnull_fd;
		stderr_fd = devnull_fd;
		if (redirect_stdout) {
			if ((stdout_fd = open(redirect_stdout, O_WRONLY | O_CREAT | O_APPEND,
					      S_IRUSR | S_IWUSR)) == -1)
				eerrorx("%s: unable to open the logfile for stdout `%s': %s",
					applet, redirect_stdout, strerror(errno));
		}
		if (redirect_stderr) {
			if ((stderr_fd = open(redirect_stderr, O_WRONLY | O_CREAT | O_APPEND,
					      S_IRUSR | S_IWUSR)) == -1)
				eerrorx("%s: unable to open the logfile for stderr `%s': %s",
					applet, redirect_stderr, strerror(errno));
		}

		/* We don't redirect stdin as some daemons may need it */
		if (background || quiet || redirect_stdout)
			dup2(stdout_fd, STDOUT_FILENO);
		if (background || quiet || redirect_stderr)
			dup2(stderr_fd, STDERR_FILENO);

		for (i = getdtablesize() - 1; i >= 3; --i)
			close(i);

		setsid();
		execv(exec, argv);
#ifdef HAVE_PAM
		if (pamr == PAM_SUCCESS)
			pam_close_session(pamh, PAM_SILENT);
#endif
		eerrorx("%s: failed to exec `%s': %s", applet, exec, strerror(errno));
	}

	/* Parent process */
	if (! background) {
		/* As we're not backgrounding the process, wait for our pid to return */
		int status = 0;
		int savepid = pid;

		errno = 0;
		do {
			pid = waitpid(savepid, &status, 0);
			if (pid < 1) {
				eerror("waitpid %d: %s", savepid, strerror(errno));
				return -1;
			}
		} while (! WIFEXITED(status) && ! WIFSIGNALED(status));

		if (! WIFEXITED(status) || WEXITSTATUS(status) != 0) {
			if (! quiet)
				eerrorx("%s: failed to start `%s'", applet, exec);
			exit(EXIT_FAILURE);
		}

		pid = savepid;
	}

	/* Wait a little bit and check that process is still running
	   We do this as some badly written daemons fork and then barf */
	if (START_WAIT > 0) {
		struct timespec ts;
		int nloops = START_WAIT / POLL_INTERVAL;
		int nloopsp = WAIT_PIDFILE / POLL_INTERVAL;
		bool alive = false;

		ts.tv_sec = 0;
		ts.tv_nsec = POLL_INTERVAL;

		while (nloops) {
			if (nanosleep(&ts, NULL) == -1) {
				if (errno == EINTR)
					eerror("%s: caught an interrupt", applet);
				else {
					eerror("%s: nanosleep: %s", applet, strerror(errno));
					return 0;
				}
			}

			/* We wait for a specific amount of time for a pidfile to be
			 * created. Once everything is in place we then wait some more
			 * to ensure that the daemon really is running and won't abort due
			 * to a config error. */
			if (! background && pidfile && nloopsp)
				nloopsp --;
			else
				nloops --;

			/* This is knarly.
			   If we backgrounded then we know the exact pid.
			   Otherwise if we have a pidfile then it *may* know the exact pid.
			   Failing that, we'll have to query processes.
			   We sleep first as some programs like ntp like to fork, and write
			   their pidfile a LONG time later. */
			if (background) {
				if (kill (pid, 0) == 0)
					alive = true;
			} else {
				if (pidfile) {
					/* The pidfile may not have been written yet - give it some time */
					if (get_pid(pidfile, true) == -1) {
						if (! nloopsp)
							eerrorx("%s: did not create a valid pid in `%s'",
								applet, pidfile);
						alive = true;
					} else
						nloopsp = 0;
				}
				if (do_stop((const char *const *)argv, cmd,
					    pidfile, uid, 0, true, false, true) > 0)
					alive = true;
			}

			if (! alive)
				eerrorx("%s: %s died", applet, exec);
		}
	}

	if (svcname)
		rc_service_daemon_set(svcname, (const char *const *)argv,
				      cmd, pidfile, true);

	exit(EXIT_SUCCESS);
	/* NOTREACHED */
}
