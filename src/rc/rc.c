/*
 * rc.c
 * rc - manager for init scripts which control the startup, shutdown
 * and the running of daemons.
 *
 * Also a multicall binary for various commands that can be used in shell
 * scripts to query service state, mark service state and provide the
 * einfo family of informational functions.
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

const char rc_copyright[] = "Copyright (c) 2007-2008 Roy Marples";

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <sys/wait.h>

#ifdef __linux__
# include <asm/setup.h> /* for COMMAND_LINE_SIZE */
#endif

#include <errno.h>
#include <dirent.h>
#include <ctype.h>
#include <getopt.h>
#include <libgen.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <strings.h>
#include <termios.h>
#include <unistd.h>

#include "builtins.h"
#include "einfo.h"
#include "rc.h"
#include "rc-logger.h"
#include "rc-misc.h"
#include "rc-plugin.h"

#include "version.h"

#define INITSH                  RC_LIBDIR "/sh/init.sh"
#define INITEARLYSH             RC_LIBDIR "/sh/init-early.sh"

#define SHUTDOWN                "/sbin/shutdown"
#define SULOGIN                 "/sbin/sulogin"

#define INTERACTIVE             RC_SVCDIR "/interactive"

#define DEVBOOT			"/dev/.rcboot"

const char *applet = NULL;
static char *runlevel;
static RC_STRINGLIST *hotplugged_services;
static RC_STRINGLIST *stop_services;
static RC_STRINGLIST *start_services;
static RC_STRINGLIST *types_n;
static RC_STRINGLIST *types_nua;
static RC_DEPTREE *deptree;
static RC_HOOK hook_out;

struct termios *termios_orig = NULL;

RC_PIDLIST service_pids;

static void
clean_failed(void)
{
	DIR *dp;
	struct dirent *d;
	size_t l;
	char *path;

	/* Clean the failed services state dir now */
	if ((dp = opendir(RC_SVCDIR "/failed"))) {
		while ((d = readdir(dp))) {
			if (d->d_name[0] == '.' &&
			    (d->d_name[1] == '\0' ||
			     (d->d_name[1] == '.' && d->d_name[2] == '\0')))
				continue;

			l = strlen(RC_SVCDIR "/failed/") +
				strlen(d->d_name) + 1;
			path = xmalloc(sizeof(char) * l);
			snprintf(path, l, RC_SVCDIR "/failed/%s", d->d_name);
			if (path) {
				if (unlink(path))
					eerror("%s: unlink `%s': %s",
					       applet, path, strerror(errno));
				free(path);
			}
		}
		closedir(dp);
	}
}

static void
cleanup(void)
{
#ifdef DEBUG_MEMORY
	RC_PID *p1 = LIST_FIRST(&service_pids);
	RC_PID *p2;
#endif

	if (!rc_in_logger && !rc_in_plugin &&
	    applet && strcmp(applet, "rc") == 0)
	{
		if (hook_out)
			rc_plugin_run(hook_out, runlevel);

		rc_plugin_unload();

		if (termios_orig) {
			tcsetattr(STDIN_FILENO, TCSANOW, termios_orig);
			free(termios_orig);
		}

		/* Clean runlevel start, stop markers */
		rmdir(RC_STARTING);
		rmdir(RC_STOPPING);
		clean_failed();
		rc_logger_close();
	}

#ifdef DEBUG_MEMORY
	while (p1) {
		p2 = LIST_NEXT(p1, entries); 
		free(p1);
		p1 = p2;
	}

	rc_stringlist_free(hotplugged_services);
	rc_stringlist_free(stop_services);
	rc_stringlist_free(start_services);
	rc_stringlist_free(types_n);
	rc_stringlist_free(types_nua);
	rc_deptree_free(deptree);
	free(runlevel);
#endif
}

#ifdef __linux__
static char *
proc_getent(const char *ent)
{
	FILE *fp;
	char proc[COMMAND_LINE_SIZE];
	char *p;
	char *value = NULL;
	int i;

	if (!exists("/proc/cmdline"))
		return NULL;

	if (!(fp = fopen("/proc/cmdline", "r"))) {
		eerror("failed to open `/proc/cmdline': %s", strerror(errno));
		return NULL;
	}

	memset(proc, 0, sizeof(proc));
	fgets(proc, sizeof(proc), fp);
	if (*proc && (p = strstr(proc, ent))) {
		i = p - proc;
		if (i == '\0' || proc[i - 1] == ' ') {
			p += strlen(ent);
			if (*p == '=')
				p++;
			value = xstrdup(strsep(&p, " "));
		}
	} else
		errno = ENOENT;
	fclose(fp);

	return value;
}
#endif

static char
read_key(bool block)
{
	struct termios termios;
	char c = 0;
	int fd = STDIN_FILENO;

	if (!isatty(fd))
		return false;

	/* Now save our terminal settings. We need to restore them at exit as we
	 * will be changing it for non-blocking reads for Interactive */
	if (!termios_orig) {
		termios_orig = xmalloc(sizeof(*termios_orig));
		tcgetattr(fd, termios_orig);
	}

	tcgetattr(fd, &termios);
	termios.c_lflag &= ~(ICANON | ECHO);
	if (block)
		termios.c_cc[VMIN] = 1;
	else {
		termios.c_cc[VMIN] = 0;
		termios.c_cc[VTIME] = 0;
	}
	tcsetattr(fd, TCSANOW, &termios);
	read(fd, &c, 1);
	tcsetattr(fd, TCSANOW, termios_orig);
	return c;
}

static bool
want_interactive(void)
{
	char c;
	static bool gotinteractive;
	static bool interactive;

	if (rc_yesno(getenv("EINFO_QUIET")))
		return false;
	if (!gotinteractive) {
		gotinteractive = true;
		interactive = rc_conf_yesno("rc_interactive");
	}
	if (!interactive)
		return false;
	c = read_key(false);
	return (c == 'I' || c == 'i') ? true : false;
}

static void
mark_interactive(void)
{
	FILE *fp = fopen(INTERACTIVE, "w");
	if (fp)
		fclose(fp);
}

static void
run_program(const char *prog)
{
	struct sigaction sa;
	sigset_t full;
	sigset_t old;
	pid_t pid;

	/* We need to block signals until we have forked */
	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = SIG_DFL;
	sigemptyset(&sa.sa_mask);
	sigfillset(&full);
	sigprocmask(SIG_SETMASK, &full, &old);
	pid = fork();

	if (pid == -1)
		eerrorx("%s: fork: %s", applet, strerror(errno));
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

		if (termios_orig)
			tcsetattr(STDIN_FILENO, TCSANOW, termios_orig);

		execl(prog, prog, (char *) NULL);
		eerror("%s: unable to exec `%s': %s", applet, prog,
		       strerror(errno));
		_exit(EXIT_FAILURE);
	}

	/* Unmask signals and wait for child */
	sigprocmask(SIG_SETMASK, &old, NULL);
	if (rc_waitpid(pid) == -1)
		eerrorx("%s: failed to exec `%s'", applet, prog);
}

static void
sulogin(bool cont)
{
#ifdef __linux__
	const char *sys = rc_sys();

	/* VSERVER and OPENVZ systems cannot do a sulogin */
	if (sys &&
	    (strcmp(sys, "VSERVER") == 0 || strcmp(sys, "OPENVZ") == 0))
	{
		execl("/sbin/halt", "/sbin/halt", "-f", (char *) NULL);
		eerrorx("%s: unable to exec `/sbin/halt': %s",
			applet, strerror(errno));
	}
#endif
	if (!cont) {
		rc_logger_close();
		exit(EXIT_SUCCESS);
	}
#ifdef __linux__
	run_program(SULOGIN);
#else
	run_program("/bin/sh");
#endif
}

_dead static void
single_user(void)
{
	rc_logger_close();
	execl(SHUTDOWN, SHUTDOWN, "now", (char *) NULL);
	eerrorx("%s: unable to exec `" SHUTDOWN "': %s",
		 applet, strerror(errno));
}

static bool
set_krunlevel(const char *level)
{
	FILE *fp;

	if (!level ||
	    strcmp(level, getenv ("RC_BOOTLEVEL")) == 0 ||
	    strcmp(level, RC_LEVEL_SINGLE) == 0 ||
	    strcmp(level, RC_LEVEL_SYSINIT) == 0)
	{
		if (exists(RC_KRUNLEVEL) &&
		    unlink(RC_KRUNLEVEL) != 0)
			eerror("unlink `%s': %s", RC_KRUNLEVEL,
			       strerror(errno));
		return false;
	}

	if (!(fp = fopen(RC_KRUNLEVEL, "w"))) {
		eerror("fopen `%s': %s", RC_KRUNLEVEL, strerror(errno));
		return false;
	}

	fprintf(fp, "%s", level);
	fclose(fp);
	return true;
}

static int
get_krunlevel(char *buffer, int buffer_len)
{
	FILE *fp;
	int i = 0;

	if (!exists(RC_KRUNLEVEL))
		return 0;
	if (!(fp = fopen(RC_KRUNLEVEL, "r"))) {
		eerror("fopen `%s': %s", RC_KRUNLEVEL, strerror(errno));
		return 0;
	}

	if (fgets(buffer, buffer_len, fp)) {
		i = strlen(buffer);
		if (buffer[i - 1] == '\n')
			buffer[i - 1] = 0;
	}
	fclose(fp);
	return i;
}

static void
add_pid(pid_t pid)
{
	RC_PID *p = xmalloc(sizeof(*p));
	p->pid = pid;
	LIST_INSERT_HEAD(&service_pids, p, entries);
}

static void
remove_pid(pid_t pid)
{
	RC_PID *p;

	LIST_FOREACH(p, &service_pids, entries)
		if (p->pid == pid) {
			LIST_REMOVE(p, entries);
			free(p);
			return;
		}
}

static void
wait_for_services(void)
{
	for (;;) {
		while (waitpid(0, 0, 0) != -1)
			;
		if (errno != EINTR)
			break;
	}
}

static void
handle_signal(int sig)
{
	int serrno = errno;
	char signame[10] = { '\0' };
	pid_t pid;
	RC_PID *pi;
	int status = 0;
	struct winsize ws;
	sigset_t sset;

	switch (sig) {
	case SIGCHLD:
		do {
			pid = waitpid(-1, &status, WNOHANG);
			if (pid < 0) {
				if (errno != ECHILD)
					eerror("waitpid: %s", strerror(errno));
				return;
			}
		} while (!WIFEXITED(status) && !WIFSIGNALED(status));

		/* Remove that pid from our list */
		if (pid > 0)
			remove_pid(pid);
		break;

	case SIGWINCH:
		if (rc_logger_tty >= 0) {
			ioctl(STDIN_FILENO, TIOCGWINSZ, &ws);
			ioctl(rc_logger_tty, TIOCSWINSZ, &ws);
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
		eerrorx("%s: caught %s, aborting", applet, signame);
		/* NOTREACHED */
	case SIGUSR1:
		eerror("rc: Aborting!");

		/* Block child signals */
		sigemptyset(&sset);
		sigaddset(&sset, SIGCHLD);
		sigprocmask(SIG_BLOCK, &sset, NULL);

		/* Kill any running services we have started */
		LIST_FOREACH(pi, &service_pids, entries)
			kill(pi->pid, SIGTERM);

		/* Notify plugins we are aborting */
		rc_plugin_run(RC_HOOK_ABORT, NULL);

		exit(EXIT_FAILURE);
		/* NOTREACHED */

	default:
		eerror("%s: caught unknown signal %d", applet, sig);
	}

	/* Restore errno */
	errno = serrno;
}

static void
do_sysinit()
{
	struct utsname uts;
	const char *sys;

	/* exec init-early.sh if it exists
	 * This should just setup the console to use the correct
	 * font. Maybe it should setup the keyboard too? */
	if (exists(INITEARLYSH))
		run_program(INITEARLYSH);

	uname(&uts);
	printf("\n   %sOpenRC %s" VERSION "%s is starting up %s",
	       ecolor(ECOLOR_GOOD), ecolor(ECOLOR_HILITE),
	       ecolor(ECOLOR_NORMAL), ecolor(ECOLOR_BRACKET));
#ifdef BRANDING
	printf(BRANDING " (%s)", uts.machine);
#else
	printf("%s %s (%s)",
	       uts.sysname,
	       uts.release,
	       uts.machine);
#endif

	if ((sys = rc_sys()))
		printf(" [%s]", sys);

	printf("%s\n\n", ecolor(ECOLOR_NORMAL));

	if (!rc_yesno(getenv ("EINFO_QUIET")) &&
	    rc_conf_yesno("rc_interactive"))
		printf("Press %sI%s to enter interactive boot mode\n\n",
		       ecolor(ECOLOR_GOOD), ecolor(ECOLOR_NORMAL));

	setenv("RC_RUNLEVEL", RC_LEVEL_SYSINIT, 1);
	run_program(INITSH);

	/* init may have mounted /proc so we can now detect or real
	 * sys */
	if ((sys = rc_sys()))
		setenv("RC_SYS", sys, 1);
}

static bool
runlevel_config(const char *service, const char *level)
{
	char *init = rc_service_resolve(service);
	char *conf, *dir;
	size_t l;
	bool retval;

	dir = dirname(init);
	dir = dirname(init);
	l = strlen(dir) + strlen(level) + strlen(service) + 10;
	conf = xmalloc(sizeof(char) * l);
	snprintf(conf, l, "%s/conf.d/%s.%s", dir, service, level);
	retval = exists(conf);
	free(conf);
	free(init);
	return retval;
}

static void
do_stop_services(const char *newlevel, bool parallel)
{
	pid_t pid;
	RC_STRING *service, *svc1, *svc2;
	RC_STRINGLIST *deporder, *tmplist;
	RC_SERVICE state;
	RC_STRINGLIST *nostop;

	if (!types_n) {
		types_n = rc_stringlist_new();
		rc_stringlist_add(types_n, "needsme");
	}

	nostop = rc_stringlist_split(rc_conf_value("rc_nostop"), " ");
	TAILQ_FOREACH_REVERSE(service, stop_services, rc_stringlist, entries)
	{
		state = rc_service_state(service->value);
		if (state & RC_SERVICE_STOPPED || state & RC_SERVICE_FAILED)
			continue;

		/* Sometimes we don't ever want to stop a service. */
		if (rc_stringlist_find(nostop, service->value)) {
			rc_service_mark(service->value, RC_SERVICE_FAILED);
			continue;
		}

		/* If we're in the start list then don't bother stopping us */
		svc1 = rc_stringlist_find(start_services, service->value);
		if (svc1) {
			if (newlevel && strcmp(runlevel, newlevel) != 0) {
				/* So we're in the start list. But we should
				 * be stopped if we have a runlevel
				 * configuration file for either the current
				 * or next so we use the correct one. */
				if (!runlevel_config(service->value,runlevel) &&
				    !runlevel_config(service->value,newlevel))
					continue;
			}
			else
				continue;
		}

		/* We got this far. Last check is to see if any any service
		 * that going to be started depends on us */
		if (!svc1) {
			tmplist = rc_stringlist_new();
			rc_stringlist_add(tmplist, service->value);
			deporder = rc_deptree_depends(deptree, types_n, tmplist,
						      newlevel ? newlevel : runlevel,
						      RC_DEP_STRICT | RC_DEP_TRACE);
			rc_stringlist_free(tmplist);
			svc2 = NULL;
			TAILQ_FOREACH(svc1, deporder, entries) {
				svc2 = rc_stringlist_find(start_services, svc1->value);
				if (svc2)
					break;
			}
			rc_stringlist_free(deporder);

			if (svc2)
				continue;
		}

		/* After all that we can finally stop the blighter! */
		pid = service_stop(service->value);
		if (pid > 0) {
			add_pid(pid);
			if (!parallel) {
				rc_waitpid(pid);
				remove_pid(pid);
			}
		}
	}

	rc_stringlist_free(nostop);
}

static void
do_start_services(bool parallel)
{
	RC_STRING *service;
	pid_t pid;
	bool interactive = false;
	RC_SERVICE state;

	if (!rc_yesno(getenv("EINFO_QUIET")))
		interactive = exists(INTERACTIVE);

	TAILQ_FOREACH(service, start_services, entries) {
		state = rc_service_state(service->value);
		if (!(state & RC_SERVICE_STOPPED) || state & RC_SERVICE_FAILED)
			continue;

		if (!interactive)
			interactive = want_interactive();

		if (interactive) {
interactive_retry:
			printf("\n");
			einfo("About to start the service %s",
			      service->value);
			eindent();
			einfo("1) Start the service\t\t2) Skip the service");
			einfo("3) Continue boot process\t\t4) Exit to shell");
			eoutdent();
interactive_option:
			switch (read_key(true)) {
				case '1': break;
				case '2': continue;
				case '3': interactive = false; break;
				case '4': sulogin(true); goto interactive_retry;
				default: goto interactive_option;
			}
		}

		pid = service_start(service->value);
		/* Remember the pid if we're running in parallel */
		if (pid > 0) {
			add_pid(pid);
			if (!parallel) {
				rc_waitpid(pid);
				remove_pid(pid);
			}
		}
	}

	/* Store our interactive status for boot */
	if (interactive &&
	    (strcmp(runlevel, RC_LEVEL_SYSINIT) == 0 ||
	     strcmp(runlevel, getenv("RC_BOOTLEVEL")) == 0))
		mark_interactive();
	else {
		if (exists(INTERACTIVE))
			unlink(INTERACTIVE);
	}

}

#ifdef RC_DEBUG
static void
handle_bad_signal(int sig)
{
	char pid[10];
	int status;
	pid_t crashed_pid = getpid();

	switch (fork()) {
		case -1:
			_exit(sig);
			/* NOTREACHED */
		case 0:
			sprintf(pid, "%i", crashed_pid);
			printf("\nAuto launching gdb!\n\n");
			_exit(execlp("gdb", "gdb", "--quiet", "--pid", pid,
				     "-ex", "bt full", NULL));
			/* NOTREACHED */
		default:
			wait(&status);
	}
	_exit(1);
	/* NOTREACHED */
}
#endif

#include "_usage.h"
#define getoptstring "o:" getoptstring_COMMON
static const struct option longopts[] = {
	{ "override", 1, NULL, 'o' },
	{ "service",  1, NULL, 's' },
	{ "sys",      0, NULL, 'S' },
	longopts_COMMON
};
static const char * const longopts_help[] = {
	"override the next runlevel to change into\nwhen leaving single user or boot runlevels",
	"runs the service specified with the rest\nof the arguments",
	"output the RC system type, if any",
	longopts_help_COMMON
};
#include "_usage.c"

int
main(int argc, char **argv)
{
	const char *bootlevel = NULL;
	char *newlevel = NULL;
	RC_STRINGLIST *deporder = NULL;
	RC_STRINGLIST *tmplist;
	RC_STRING *service;
	bool going_down = false;
	int depoptions = RC_DEP_STRICT | RC_DEP_TRACE;
	char krunlevel [PATH_MAX];
	char pidstr[10];
	int opt;
	bool parallel;
	int regen = 0;
#ifdef __linux__
	char *proc;
	char *p;
	char *token;
#endif

#ifdef RC_DEBUG
	signal_setup(SIGBUS, handle_bad_signal);
	signal_setup(SIGILL, handle_bad_signal);
	signal_setup(SIGSEGV, handle_bad_signal);
#endif

	applet = basename_c(argv[0]);
	LIST_INIT(&service_pids);
	atexit(cleanup);
	if (!applet)
		eerrorx("arguments required");

	if (argc > 1 && (strcmp(argv[1], "--version") == 0)) {
		printf("%s (OpenRC", applet);
		if ((bootlevel = rc_sys()))
			printf(" [%s]", bootlevel);
		printf(") " VERSION
#ifdef BRANDING
		       " (" BRANDING ")"
#endif
		       "\n");
		exit(EXIT_SUCCESS);
	}

	/* Run our built in applets. If we ran one, we don't return. */
	run_applets(argc, argv);

	argc--;
	argv++;

	/* Change dir to / to ensure all scripts don't use stuff in pwd */
	chdir("/");

	/* Ensure our environment is pure
	 * Also, add our configuration to it */
	env_filter();
	env_config();

	argc++;
	argv--;
	while ((opt = getopt_long(argc, argv, getoptstring,
				  longopts, (int *) 0)) != -1)
	{
		switch (opt) {
		case 'o':
			if (*optarg == '\0')
				optarg = NULL;
			if (!rc_runlevel_exists(optarg)) {
				eerror("runlevel `%s' does not exist", optarg);
				exit(EXIT_FAILURE);
			}
			if (!set_krunlevel(optarg))
				exit(EXIT_FAILURE);
			einfo("Overriding next runlevel to %s", optarg);
			exit(EXIT_SUCCESS);
			/* NOTREACHED */
		case 's':
			newlevel = rc_service_resolve(optarg);
			if (!newlevel)
				eerrorx("%s: service `%s' does not exist",
					applet, optarg);
			argv += optind - 1;
			*argv = newlevel;
			execv(*argv, argv);
			eerrorx("%s: %s", applet, strerror(errno));
			/* NOTREACHED */
		case 'S':
			bootlevel = rc_sys();
			if (bootlevel)
				printf("%s\n", bootlevel);
			exit(EXIT_SUCCESS);
			/* NOTREACHED */
		case_RC_COMMON_GETOPT
		}
	}

	newlevel = argv[optind++];
	/* To make life easier, we only have the shutdown runlevel as
	 * nothing really needs to know that we're rebooting.
	 * But for those that do, you can test against RC_REBOOT. */
	if (newlevel) {
		if (strcmp(newlevel, "reboot") == 0) {
			newlevel = UNCONST(RC_LEVEL_SHUTDOWN);
			setenv("RC_REBOOT", "YES", 1);
		}
	}

	/* Enable logging */
	setenv("EINFO_LOG", "rc", 1);

	/* Export our PID */
	snprintf(pidstr, sizeof(pidstr), "%d", getpid());
	setenv("RC_PID", pidstr, 1);

	/* Load current runlevel */
	bootlevel = getenv("RC_BOOTLEVEL");
	runlevel = rc_runlevel_get();

	rc_logger_open(newlevel ? newlevel : runlevel);

	/* Setup a signal handler */
	signal_setup(SIGINT, handle_signal);
	signal_setup(SIGQUIT, handle_signal);
	signal_setup(SIGTERM, handle_signal);
	signal_setup(SIGUSR1, handle_signal);
	signal_setup(SIGWINCH, handle_signal);

	/* Run any special sysinit foo */
	if (newlevel && strcmp(newlevel, RC_LEVEL_SYSINIT) == 0) {
		do_sysinit();
		free(runlevel);
		runlevel = rc_runlevel_get();
	}

	rc_plugin_load();

	/* Now we start handling our children */
	signal_setup(SIGCHLD, handle_signal);

	if (newlevel &&
	    (strcmp(newlevel, RC_LEVEL_SHUTDOWN) == 0 ||
	     strcmp(newlevel, RC_LEVEL_SINGLE) == 0))
	{
		going_down = true;
		if (!exists(RC_KRUNLEVEL))
			set_krunlevel(runlevel);
		rc_runlevel_set(newlevel);
		setenv("RC_RUNLEVEL", newlevel, 1);
		setenv("RC_GOINGDOWN", "YES", 1);
	} else {
		/* We should not use krunevel in sysinit or the boot runlevel */
		if (!newlevel ||
		    (strcmp(newlevel, RC_LEVEL_SYSINIT) != 0 &&
		     strcmp(newlevel, getenv("RC_BOOTLEVEL")) != 0))
		{
			if (get_krunlevel(krunlevel, sizeof(krunlevel))) {
				newlevel = krunlevel;
				set_krunlevel(NULL);
			}
		}

		if (newlevel) {
			if (strcmp(runlevel, newlevel) != 0 &&
			    !rc_runlevel_exists(newlevel))
				eerrorx("%s: not a valid runlevel", newlevel);

#ifdef __linux__
			if (strcmp(newlevel, RC_LEVEL_SYSINIT) == 0) {
				/* If we requested a runlevel, save it now */
				p = proc_getent("rc_runlevel");
				if (p == NULL)
					p = proc_getent("softlevel");
				if (p != NULL) {
					set_krunlevel(p);
					free(p);
				}
			}
#endif
		}
	}

	if (going_down) {
#ifdef __FreeBSD__
		/* FIXME: we shouldn't have todo this */
		/* For some reason, wait_for_services waits for the logger
		 * proccess to finish as well, but only on FreeBSD.
		 * We cannot allow this so we stop logging now. */
		rc_logger_close();
#endif

		rc_plugin_run(RC_HOOK_RUNLEVEL_STOP_IN, newlevel);
	} else {
		rc_plugin_run(RC_HOOK_RUNLEVEL_STOP_IN, runlevel);
	}
	hook_out = RC_HOOK_RUNLEVEL_STOP_OUT;

	/* Check if runlevel is valid if we're changing */
	if (newlevel && strcmp(runlevel, newlevel) != 0 && !going_down) {
		if (!rc_runlevel_exists(newlevel))
			eerrorx("%s: is not a valid runlevel", newlevel);
	}

	/* Load our deptree */
	if ((deptree = _rc_deptree_load(0, &regen)) == NULL)
		eerrorx("failed to load deptree");
	if (exists(RC_DEPTREE_SKEWED))
		ewarn("WARNING: clock skew detected!");

	/* Clean the failed services state dir */
	clean_failed();

	if (mkdir(RC_STOPPING, 0755) != 0) {
		if (errno == EACCES)
			eerrorx("%s: superuser access required", applet);
		eerrorx("%s: failed to create stopping dir `%s': %s",
			applet, RC_STOPPING, strerror(errno));
	}

	/* Build a list of all services to stop and then work out the
	 * correct order for stopping them */
	stop_services = rc_services_in_state(RC_SERVICE_STARTED);
	tmplist = rc_services_in_state(RC_SERVICE_INACTIVE);
	TAILQ_CONCAT(stop_services, tmplist, entries);
	free(tmplist);
	tmplist = rc_services_in_state(RC_SERVICE_STARTING);
	TAILQ_CONCAT(stop_services, tmplist, entries);
	free(tmplist);
	if (stop_services)
		rc_stringlist_sort(&stop_services);

	types_nua = rc_stringlist_new();
	rc_stringlist_add(types_nua, "ineed");
	rc_stringlist_add(types_nua, "iuse");
	rc_stringlist_add(types_nua, "iafter");

	if (stop_services) {
		tmplist = rc_deptree_depends(deptree, types_nua, stop_services,
				runlevel, depoptions | RC_DEP_STOP);
		rc_stringlist_free(stop_services);
		stop_services = tmplist;
	}

	/* Load our list of start services */
	hotplugged_services = rc_services_in_state(RC_SERVICE_HOTPLUGGED);
	start_services = rc_services_in_runlevel(newlevel ?
						 newlevel : runlevel);
	if (strcmp(newlevel ? newlevel : runlevel, RC_LEVEL_SHUTDOWN) != 0 &&
	    strcmp(newlevel ? newlevel : runlevel, RC_LEVEL_SYSINIT) != 0)
	{
		tmplist = rc_services_in_runlevel(RC_LEVEL_SYSINIT);
		TAILQ_CONCAT(start_services, tmplist, entries);
		free(tmplist);
		if (strcmp(newlevel ? newlevel : runlevel,
			   RC_LEVEL_SINGLE) != 0)
		{
			if (strcmp(newlevel ? newlevel : runlevel,
				   bootlevel) != 0)
			{
				tmplist = rc_services_in_runlevel(bootlevel);
				TAILQ_CONCAT(start_services, tmplist, entries);
				free(tmplist);
			}
			if (hotplugged_services) {
				TAILQ_FOREACH(service, hotplugged_services,
					      entries)
					rc_stringlist_addu(start_services,
							   service->value);
			}
		}
	}

	parallel = rc_conf_yesno("rc_parallel");

	/* Now stop the services that shouldn't be running */
	if (stop_services)
		do_stop_services(newlevel, parallel);

	/* Wait for our services to finish */
	wait_for_services();

	/* Notify the plugins we have finished */
	rc_plugin_run(RC_HOOK_RUNLEVEL_STOP_OUT,
		      going_down ? newlevel : runlevel);
	hook_out = 0;

	rmdir(RC_STOPPING);

	/* Store the new runlevel */
	if (newlevel) {
		rc_runlevel_set(newlevel);
		free(runlevel);
		runlevel = xstrdup(newlevel);
		setenv("RC_RUNLEVEL", runlevel, 1);
	}

#ifdef __linux__
	/* We can't log beyond this point as the shutdown runlevel
	 * will mount / readonly. */
	if (strcmp(runlevel, RC_LEVEL_SHUTDOWN) == 0)
		rc_logger_close();
#endif

	mkdir(RC_STARTING, 0755);
	rc_plugin_run(RC_HOOK_RUNLEVEL_START_IN, runlevel);
	hook_out = RC_HOOK_RUNLEVEL_START_OUT;

	/* Re-add our hotplugged services if they stopped */
	if (hotplugged_services)
		TAILQ_FOREACH(service, hotplugged_services, entries)
			rc_service_mark(service->value, RC_SERVICE_HOTPLUGGED);

	/* Order the services to start */
	if (start_services) {
		rc_stringlist_sort(&start_services);
		deporder = rc_deptree_depends(deptree, types_nua,
					      start_services, runlevel,
					      depoptions | RC_DEP_START);
		rc_stringlist_free(start_services);
		start_services = deporder;
	}

#ifdef __linux__
	/* mark any services skipped as started */
	proc = p = proc_getent("noinit");
	if (proc) {
		while ((token = strsep(&p, ",")))
			rc_service_mark(token, RC_SERVICE_STARTED);
		free(proc);
	}
#endif

	if (start_services) {
		do_start_services(parallel);
		/* FIXME: If we skip the boot runlevel and go straight
		 * to default from sysinit, we should now re-evaluate our
		 * start services + hotplugged services and call
		 * do_start_services a second time. */

		/* Wait for our services to finish */
		wait_for_services();
	}

#ifdef __linux__
	/* mark any services skipped as stopped */
	proc = p = proc_getent("noinit");
	if (proc) {
		while ((token = strsep(&p, ",")))
			rc_service_mark(token, RC_SERVICE_STOPPED);
		free(proc);
	}

#endif

	rc_plugin_run(RC_HOOK_RUNLEVEL_START_OUT, runlevel);
	hook_out = 0;

	/* If we're in the boot runlevel and we regenerated our dependencies
	 * we need to delete them so that they are regenerated again in the
	 * default runlevel as they may depend on things that are now
	 * available */
	if (regen && strcmp(runlevel, bootlevel) == 0)
		unlink(RC_DEPTREE_CACHE);

	return EXIT_SUCCESS;
}
