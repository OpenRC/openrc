/*
 * supervise-daemon
 * This is a supervisor for daemons.
 * It will start a daemon and make sure it restarts if it crashes.
 */

/*
 * Copyright (c) 2025 The OpenRC Authors.
 * See the Authors file at the top-level directory of this distribution and
 * https://github.com/OpenRC/openrc/blob/HEAD/AUTHORS
 *
 * This file is part of OpenRC. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution and at https://github.com/OpenRC/openrc/blob/HEAD/LICENSE
 * This file may not be copied, modified, propagated, or distributed
 *    except according to the terms contained in the LICENSE file.
 */

#include <sys/socket.h>
#include <sys/un.h>

#include <poll.h>
#include <queue.h>
#include <signal.h>
#include <stddef.h>
#include <syslog.h>
#include <unistd.h>

#include <einfo.h>
#include <rc.h>

#include "daemon.h"
#include "_usage.h"

const char *applet = NULL;
const char *extraopts = NULL;
const char getoptstring[] = getoptstring_COMMON;
const struct option longopts[] = { longopts_COMMON };
const char * const longopts_help[] = { longopts_help_COMMON };
const char *usagestring = NULL;

static bool verbose = false;

extern char **environ;

struct supervisor {
	pid_t pid;
	char *svcname;
	struct supervisor *prev, *next;
} *root;

int signal_pipe[2];

static void
handle_sigchld(int sig, siginfo_t *info, RC_UNUSED void *ctx)
{
	if (sig != SIGCHLD)
		return;

	write(signal_pipe[1], info, sizeof(*info));
}

static void
add_to_tree(pid_t pid, const char *svcname)
{
	struct supervisor *entry = xmalloc(sizeof(*entry));
	*entry = (struct supervisor) {
		.pid = pid,
		.svcname = xstrdup(svcname),
		.next = root
	};

	if (root)
		root->prev = entry;

	root = entry;
}

static void
setup_environment(void)
{
	struct sigaction sa = { .sa_sigaction = handle_sigchld };
	RC_STRINGLIST *env_list = rc_stringlist_new();
	RC_STRING *env;
	sigset_t signals;
	const char *path;
	int fd;

	openlog(applet, LOG_PID, LOG_DAEMON);

	/* Clean the environment of any RC_ variables */
	for (size_t i = 0; environ[i]; i++)
		rc_stringlist_add(env_list, environ[i++]);

	TAILQ_FOREACH(env, env_list, entries) {
		if (strncmp(env->value, "RC_", 3) == 0 && strncmp(env->value, "SSD_", 4) == 0) {
			*strchr(env->value, '=') = '\0';
			unsetenv(env->value);
		}
	}
	rc_stringlist_free(env_list);

	/* For the path, remove the rcscript bin dir from it */
	if ((path = getenv("PATH"))) {
		const char *entry;
		char *newpath;
		size_t len;

		FILE *fp = xopen_memstream(&newpath, &len);

		while ((entry = strchr(path, ':'))) {
			if (strncmp(RC_LIBEXECDIR "/bin", path, entry - path) != 0
					|| strncmp(RC_LIBEXECDIR "/sbin", path, entry - path) != 0) {
				fprintf(fp, "%*.s:", (int)(entry - path), path);
			}
			while (*entry == ':')
				entry++;
			path = entry;
		}

		xclose_memstream(fp);
		if (newpath[len - 1] == ':')
			newpath[len - 1] = '\0';

		setenv("PATH", newpath, true);
	}

	umask(022);

#ifdef TIOCNOTTY
	/* remove the controlling tty */
	fd = open("/dev/tty", O_RDWR);
	ioctl(fd, TIOCNOTTY, 0);
	close(fd);
#endif

	fd = open("/dev/null", O_RDWR);
	dup2(fd, STDIN_FILENO);
	dup2(fd, STDOUT_FILENO);
	dup2(fd, STDERR_FILENO);
	close(fd);

	sigfillset(&signals);
	sigdelset(&signals, SIGCHLD);
	sigprocmask(SIG_SETMASK, &signals, NULL);
	sigaction(SIGCHLD, &sa, NULL);

	fchdir(rc_dirfd(RC_DIR_DAEMONS));
}

static int
setup_fifo(void)
{
	int fd;

	/* TODO: handle this better. */
	if (mkfifo(applet, 0700) == -1) {
		if (errno != EEXIST)
			exit(EXIT_FAILURE);
		unlink(applet);
		if (mkfifo(applet, 0700) == -1)
			exit(EXIT_FAILURE);
	}

	if ((fd = open(applet, O_RDONLY)) == -1)
		exit(EXIT_FAILURE);

	return fd;
}

extern int socket_fd;

static pid_t
start_supervisor(int socket, const char *service)
{
	sigset_t signals, old;
	pid_t pid;

	sigfillset(&signals);
	sigprocmask(SIG_SETMASK, &signals, &old);
	if ((pid = fork()) == 0) {
		socket_fd = socket;
		supervise(service);
	}
	sigprocmask(SIG_SETMASK, &old, NULL);
	return pid;
}

static void
handle_command(int fifo)
{
	pid_t pid;
	char service[BUFSIZ];
	int count = read(fifo, service, sizeof(service) - 1);

	if (count <= 0)
		return;

	service[count] = '\0';
	if (!rc_service_exists(service)) {
		return;
	}

	pid = start_supervisor(fifo, service);
	add_to_tree(pid, service);
}

static void
handle_signal_pipe(int pipefd)
{
	struct supervisor *iter = root;
	siginfo_t info;
	if (read(pipefd, &info, sizeof(info)) != sizeof(info))
		return;
	if (waitpid(info.si_pid, NULL, WNOHANG) == -1)
		return;
	while (iter && iter->pid != info.si_pid)
		iter = iter->next;
	if (!iter)
		return;
	iter->pid = start_supervisor(-1, iter->svcname);
}

int
main(RC_UNUSED int argc, char **argv)
{
	int opt;

	enum { CONTROL, SIGNAL };
	struct pollfd pfds[] = {
		[CONTROL] = { .events = POLLIN },
		[SIGNAL] = { .events = POLLIN }
	};

	applet = basename_c(argv[0]);
	verbose = rc_yesno(getenv("EINFO_VERBOSE"));

	setup_environment();

	/* i'm assuming we don't want to block in a self
	 * pipe inside the signal handler. */
	if (pipe2(signal_pipe, O_CLOEXEC | O_NONBLOCK) == -1)
		return EXIT_FAILURE;
	pfds[SIGNAL].fd = signal_pipe[0];

	if (rc_yesno(getenv("RC_USER_SERVICES")))
		rc_set_user();

	while ((opt = getopt_long(argc, argv, getoptstring, longopts, NULL)) != -1)
		switch (opt) {
		case_RC_COMMON_GETOPT
		}

	if ((pfds[CONTROL].fd = setup_fifo()))
		return EXIT_FAILURE;
	for (;;) {
		if (poll(pfds, ARRAY_SIZE(pfds), -1) == -1)
			continue;

		if (pfds[CONTROL].revents & POLLIN)
			handle_command(pfds[CONTROL].fd);
		if (pfds[SIGNAL].revents & POLLIN)
			handle_signal_pipe(pfds[SIGNAL].fd);
	}
}
