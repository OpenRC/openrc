/*
 * openrc-init.c
 * This is the init process (pid 1) for OpenRC.
 *
 * This is based on code written by James Hammons <jlhamm@acm.org>, so
 * I would like to publically thank him for his work.
 */

/*
 * Copyright (c) 2017 The OpenRC Authors.
 * See the Authors file at the top-level directory of this distribution and
 * https://github.com/OpenRC/openrc/blob/master/AUTHORS
 *
 * This file is part of OpenRC. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution and at https://github.com/OpenRC/openrc/blob/master/LICENSE
 * This file may not be copied, modified, propagated, or distributed
 *    except according to the terms contained in the LICENSE file.
 */

#include <errno.h>
#include <pwd.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/reboot.h>
#include <sys/wait.h>

#ifdef HAVE_SELINUX
#  include <selinux/selinux.h>
#endif

#include "helpers.h"
#include "rc.h"
#include "rc-plugin.h"
#include "rc-wtmp.h"
#include "version.h"

static const char *path_default = "/sbin:/usr/sbin:/bin:/usr/bin";
static const char *rc_default_runlevel = "default";

static void do_openrc(const char *runlevel)
{
	pid_t pid;
	sigset_t all_signals;
	sigset_t our_signals;

	sigfillset(&all_signals);
	/* block all signals */
	sigprocmask(SIG_BLOCK, &all_signals, &our_signals);
	pid = fork();
	switch (pid) {
		case -1:
			perror("fork");
			exit(1);
			break;
		case 0:
			setsid();
			/* unblock all signals */
			sigprocmask(SIG_UNBLOCK, &all_signals, NULL);
			printf("Starting %s runlevel\n", runlevel);
			execlp("openrc", "openrc", runlevel, NULL);
			perror("exec");
			exit(1);
			break;
		default:
			/* restore our signal mask */
			sigprocmask(SIG_SETMASK, &our_signals, NULL);
			while (waitpid(pid, NULL, 0) != pid)
				if (errno == ECHILD)
					break;
			break;
	}
}

static void init(const char *default_runlevel)
{
	const char *runlevel = NULL;
	do_openrc("sysinit");
	do_openrc("boot");
	if (default_runlevel)
		runlevel = default_runlevel;
	else
		runlevel = rc_conf_value("rc_default_runlevel");
	if (!runlevel)
		runlevel = rc_default_runlevel;
	if (!rc_runlevel_exists(runlevel)) {
		printf("%s is an invalid runlevel\n", runlevel);
		runlevel = rc_default_runlevel;
	}
	do_openrc(runlevel);
	log_wtmp("reboot", "~~", 0, RUN_LVL, "~~");
}

static void handle_reexec(char *my_name)
{
	execlp(my_name, my_name, "reexec", NULL);
	return;
}

static void handle_shutdown(const char *runlevel, int cmd)
{
	struct timespec ts;

	do_openrc(runlevel);
	printf("Sending the final term signal\n");
	kill(-1, SIGTERM);
	ts.tv_sec = 3;
	ts.tv_nsec = 0;
	nanosleep(&ts, NULL);
	printf("Sending the final kill signal\n");
	kill(-1, SIGKILL);
	sync();
	reboot(cmd);
}

static void run_program(const char *prog)
{
	sigset_t full;
	sigset_t old;
	pid_t pid;

	/* We need to block signals until we have forked */
	sigfillset(&full);
	sigprocmask(SIG_SETMASK, &full, &old);
	pid = fork();
	if (pid == -1) {
		perror("init");
		return;
	}
	if (pid == 0) {
		/* Unmask signals */
		sigprocmask(SIG_SETMASK, &old, NULL);
		execl(prog, prog, (char *)NULL);
		perror("init");
		exit(1);
	}
	/* Unmask signals and wait for child */
	sigprocmask(SIG_SETMASK, &old, NULL);
	if (rc_waitpid(pid) == -1)
		perror("init");
}

static void open_shell(void)
{
	const char *shell;
	struct passwd *pw;

#ifdef __linux__
	const char *sys = rc_sys();

	/* VSERVER systems cannot really drop to shells */
	if (sys && strcmp(sys, RC_SYS_VSERVER) == 0)
	{
		execlp("halt", "halt", "-f", (char *) NULL);
		perror("init");
		return;
	}
#endif

	shell = rc_conf_value("rc_shell");
	/* No shell set, so obey env, then passwd, then default to /bin/sh */
	if (!shell) {
		shell = getenv("SHELL");
		if (!shell) {
			pw = getpwuid(getuid());
			if (pw)
				shell = pw->pw_shell;
			if (!shell)
				shell = "/bin/sh";
		}
	}
	run_program(shell);
}

static void handle_single(void)
{
	do_openrc("single");
}

static void reap_zombies(void)
{
	pid_t pid;

	for (;;) {
		pid = waitpid(-1, NULL, WNOHANG);
		if (pid == 0)
			break;
		else if (pid == -1) {
			if (errno == ECHILD)
				break;
			perror("waitpid");
			continue;
		}
	}
}

static void signal_handler(int sig)
{
	switch (sig) {
		case SIGINT:
			handle_shutdown("reboot", RB_AUTOBOOT);
			break;
		case SIGTERM:
#ifdef SIGPWR
		case SIGPWR:
#endif
			handle_shutdown("shutdown", RB_HALT_SYSTEM);
			break;
		case SIGCHLD:
			reap_zombies();
			break;
		default:
			printf("Unknown signal received, %d\n", sig);
			break;
	}
}

int main(int argc, char **argv)
{
	char *default_runlevel;
	char buf[2048];
	int count;
	FILE *fifo;
	bool reexec = false;
	sigset_t signals;
	struct sigaction sa;
#ifdef HAVE_SELINUX
	int			enforce = 0;
#endif

	if (getpid() != 1)
		return 1;

#ifdef HAVE_SELINUX
	if (getenv("SELINUX_INIT") == NULL) {
		if (is_selinux_enabled() != 1) {
			if (selinux_init_load_policy(&enforce) == 0) {
				putenv("SELINUX_INIT=YES");
				execv(argv[0], argv);
			} else {
				if (enforce > 0) {
					/*
					 * SELinux in enforcing mode but load_policy failed
					 * At this point, we probably can't open /dev/console,
					 * so log() won't work
					 */
					fprintf(stderr,"Unable to load SELinux Policy.\n");
					fprintf(stderr,"Machine is  in enforcing mode.\n");
					fprintf(stderr,"Halting now.\n");
					exit(1);
				}
			}
		}
	}
#endif

	printf("OpenRC init version %s starting\n", VERSION);

	if (argc > 1)
		default_runlevel = argv[1];
	else
		default_runlevel = NULL;

	if (default_runlevel && strcmp(default_runlevel, "reexec") == 0)
		reexec = true;

	/* block all signals we do not handle */
	sigfillset(&signals);
	sigdelset(&signals, SIGCHLD);
	sigdelset(&signals, SIGINT);
	sigdelset(&signals, SIGTERM);
#ifdef SIGPWR
	sigdelset(&signals, SIGPWR);
#endif
	sigprocmask(SIG_SETMASK, &signals, NULL);

	/* install signal  handler */
	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = signal_handler;
	sigaction(SIGCHLD, &sa, NULL);
	sigaction(SIGINT, &sa, NULL);
	sigaction(SIGTERM, &sa, NULL);
#ifdef SIGPWR
	sigaction(SIGPWR, &sa, NULL);
#endif
	reboot(RB_DISABLE_CAD);

	/* set default path */
	setenv("PATH", path_default, 1);

	if (! reexec)
		init(default_runlevel);

	if (mkfifo(RC_INIT_FIFO, 0600) == -1 && errno != EEXIST)
		perror("mkfifo");

	for (;;) {
		/* This will block until a command is sent down the pipe... */
		fifo = fopen(RC_INIT_FIFO, "r");
		if (!fifo) {
			if (errno != EINTR)
				perror("fopen");
			continue;
		}
		count = fread(buf, 1, sizeof(buf) - 1, fifo);
		buf[count] = 0;
		fclose(fifo);
		printf("PID1: Received \"%s\" from FIFO...\n", buf);
		if (strcmp(buf, "halt") == 0)
			handle_shutdown("shutdown", RB_HALT_SYSTEM);
		else if (strcmp(buf, "kexec") == 0)
			handle_shutdown("reboot", RB_KEXEC);
		else if (strcmp(buf, "poweroff") == 0)
			handle_shutdown("shutdown", RB_POWER_OFF);
		else if (strcmp(buf, "reboot") == 0)
			handle_shutdown("reboot", RB_AUTOBOOT);
		else if (strcmp(buf, "reexec") == 0)
			handle_reexec(argv[0]);
		else if (strcmp(buf, "single") == 0) {
			handle_single();
			open_shell();
			init(default_runlevel);
		}
	}
	return 0;
}
