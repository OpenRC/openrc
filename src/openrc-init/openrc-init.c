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
 * https://github.com/OpenRC/openrc/blob/HEAD/AUTHORS
 *
 * This file is part of OpenRC. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution and at https://github.com/OpenRC/openrc/blob/HEAD/LICENSE
 * This file may not be copied, modified, propagated, or distributed
 *    except according to the terms contained in the LICENSE file.
 */

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <pwd.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/reboot.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <utmp.h>

#ifdef HAVE_SELINUX
#  include <selinux/selinux.h>
#endif

#include "rc.h"
#include "helpers.h"
#include "plugin.h"
#include "wtmp.h"
#include "version.h"

static const char *default_runlevel = NULL;
static const char *my_name = "openrc-init";
static const char *path_default = "/sbin:/usr/sbin:/bin:/usr/bin";
static const char * const rc_default_runlevel = "default";

/* wait for children until a signal occurs or we run out of children */
static pid_t wait_children(int options)
{
	pid_t pid;
	do {
		pid = waitpid(-1, NULL, options);
	} while (pid > 0);
	return pid < 0 ? -errno : pid;
}

/* wait for children to exit, stop when a specific child exits */
static pid_t wait_child(pid_t child)
{
	pid_t pid;
	do {
		pid = waitpid(-1, NULL, 0);
	} while ((pid > 0 && pid != child) || (pid < 0 && errno == EINTR));
	return pid < 0 ? -errno : pid;
}

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
			wait_child(pid);
			break;
	}
}

static void init()
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

static void handle_reexec()
{
	execlp(my_name, my_name, "reexec", NULL);
	return;
}

static void alarm_handler(int signum RC_UNUSED) {
	/* do nothing */
}

static void handle_shutdown(const char *runlevel, int cmd)
{
	do_openrc(runlevel);

	/* wait on any children that have already exited */
	if (wait_children(WNOHANG) != -ECHILD) {
		pid_t pid;
		sigset_t signals;
		struct sigaction sa = { .sa_handler = alarm_handler };

		sigaction(SIGALRM, &sa, NULL);

		sigfillset(&signals);
		sigdelset(&signals, SIGALRM);
		sigprocmask(SIG_SETMASK, &signals, NULL);

		printf("Sending the final term signal\n");
		kill(-1, SIGTERM);

		/* Wait up to 3 seconds for children to exit */
		alarm(3);
		pid = wait_children(0);
		alarm(0);

		if (pid != -ECHILD) {
			printf("Sending the final kill signal\n");
			kill(-1, SIGKILL);

			alarm(3);
			wait_children(0);
			alarm(0);
		}
	}

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
	if (wait_child(pid) < 0)
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

static volatile sig_atomic_t received_signals[64] = { 0 };

static bool check_signal(int signum)
{
	assert(signum >= 1 && signum <= 64);
	if (received_signals[signum - 1]) {
		received_signals[signum - 1] = 0;
		return true;
	}
	return false;
}

static void signal_handler(int signum)
{
	assert(signum >= 1 && signum <= 64);
	received_signals[signum - 1] = 1;
}

static void setup_signal(sigset_t *signals, int signum)
{
	struct sigaction sa = { .sa_handler = signal_handler };
	sigaction(signum, &sa, NULL);
	sigdelset(signals, signum);
}

static void setup_signals()
{
	sigset_t signals;
	sigfillset(&signals);
	setup_signal(&signals, SIGCHLD);
	setup_signal(&signals, SIGINT);
	setup_signal(&signals, SIGTERM);
#ifdef SIGPWR
	setup_signal(&signals, SIGPWR);
#endif
	setup_signal(&signals, SIGRTMIN+3);
	setup_signal(&signals, SIGRTMIN+4);
	setup_signal(&signals, SIGRTMIN+5);
	setup_signal(&signals, SIGRTMIN+6);
	setup_signal(&signals, SIGRTMIN+13);
	setup_signal(&signals, SIGRTMIN+14);
	setup_signal(&signals, SIGRTMIN+15);
	setup_signal(&signals, SIGRTMIN+16);
	sigprocmask(SIG_SETMASK, &signals, NULL);
}

static void process_signals()
{
	if (check_signal(SIGRTMIN+16))
		reboot(RB_KEXEC);

	if (check_signal(SIGRTMIN+15))
		reboot(RB_AUTOBOOT);

	if (check_signal(SIGRTMIN+14))
		reboot(RB_POWER_OFF);

	if (check_signal(SIGRTMIN+13))
		reboot(RB_HALT_SYSTEM);

	if (check_signal(SIGRTMIN+6))
		handle_shutdown("reboot", RB_KEXEC);

	if (check_signal(SIGRTMIN+5))
		handle_shutdown("reboot", RB_AUTOBOOT);

	if (check_signal(SIGINT))
		handle_shutdown("reboot", RB_AUTOBOOT);

	if (check_signal(SIGRTMIN+4))
		handle_shutdown("shutdown", RB_POWER_OFF);

	if (check_signal(SIGTERM))
		handle_shutdown("shutdown", RB_POWER_OFF);
#ifdef SIGPWR
	if (check_signal(SIGPWR))
		handle_shutdown("shutdown", RB_HALT_SYSTEM);
#endif
	if (check_signal(SIGRTMIN+3))
		handle_shutdown("shutdown", RB_HALT_SYSTEM);

	if (check_signal(SIGCHLD))
		wait_children(WNOHANG);
}

static void read_fifo()
{
	static int fifo = -1;
	ssize_t count;
	char buf[10];

	if (fifo < 0)
		/* This will block until a process opens the fifo for writing */
		fifo = open(RC_INIT_FIFO, O_RDONLY|O_CLOEXEC);

	if (fifo < 0) {
		if (errno != EINTR)
			fprintf(stderr, "%s: open(%s): %s\n", my_name,
				RC_INIT_FIFO, strerror(errno));
		return;
	}

	count = read(fifo, buf, sizeof(buf) - 1);

	if (count < 0) {
		if (errno != EINTR)
			fprintf(stderr, "%s: read(%s): %s\n", my_name,
				RC_INIT_FIFO, strerror(errno));
		/* Keep the fifo open to avoid sending SIGPIPE to the writer */
		return;
	}

	buf[count] = 0;

	close(fifo);
	fifo = -1;

	if (count == 0)
		/* Another process opened the fifo without writing anything */
		return;

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
		handle_reexec();
	else if (strcmp(buf, "single") == 0) {
		handle_single();
		open_shell();
		init();
	}
}

int main(int argc, char **argv)
{
	bool reexec = false;
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

	if (argc > 0)
		my_name = argv[0];

	if (argc > 1)
		default_runlevel = argv[1];

	if (default_runlevel && strcmp(default_runlevel, "reexec") == 0)
		reexec = true;

	setup_signals();

	reboot(RB_DISABLE_CAD);

	/* set default path */
	setenv("PATH", path_default, 1);

	if (!reexec)
		init();

	if (mkfifo(RC_INIT_FIFO, 0600) == -1 && errno != EEXIST)
		perror("mkfifo");

	for (;;) {
		process_signals();
		read_fifo();
	}
	return 0;
}
