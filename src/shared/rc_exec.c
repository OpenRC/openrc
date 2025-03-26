/*
 * Copyright (c) 2025 The OpenRC Authors.
 * See the Authors file at the top-level directory of this distribution and
 * https://github.com/OpenRC/openrc/blob/HEAD/AUTHORS
 *
 * This file is part of OpenRC. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution and at https://github.com/OpenRC/openrc/blob/HEAD/LICENSE
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file.
 */

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

#include "rc_exec.h"
#include "helpers.h"

static void checked_close(int fd)
{
	if (fd >= 0)
		close(fd);
}

static int devnull(void)
{
	static int devnullfd = -1;
	if (devnullfd < 0)
		devnullfd = open("/dev/null", O_RDWR | O_CLOEXEC);
	if (devnullfd < 0) {
		fprintf(stderr, "Failed to open /dev/null: %s", strerror(errno));
		_exit(1);
	}
	return devnullfd;
}

struct exec_args exec_init(const char **argv)
{
	struct exec_args args = {0};
	args.argv = argv;
	args.redirect_stdin  = EXEC_NO_REDIRECT;
	args.redirect_stdout = EXEC_NO_REDIRECT;
	args.redirect_stderr = EXEC_NO_REDIRECT;
	args.uid = (uid_t)-1;
	args.gid = (gid_t)-1;
	return args;
}

struct exec_result do_exec(struct exec_args *args)
{
	struct exec_result res = { .pid = -1 };
	sigset_t full, old;
	int saved_errno, err, n;
	int execpipe[2] = {-1, -1};
	int stdin_pipe[2] = {-1, -1};
	int stdout_pipe[2] = {-1, -1};
	int stderr_pipe[2] = {-1, -1};
	const char *cmd = args->cmd ? args->cmd : args->argv[0];

	if (pipe2(execpipe, O_CLOEXEC) < 0)
		goto exit;
	if (args->redirect_stdin == EXEC_MKPIPE) {
		if (pipe2(stdin_pipe, O_CLOEXEC) < 0)
			goto exit;
		args->redirect_stdin = stdin_pipe[0];
	}
	if (args->redirect_stdout == EXEC_MKPIPE) {
		if (pipe2(stdout_pipe, O_CLOEXEC) < 0)
			goto exit;
		args->redirect_stdout = stdout_pipe[1];
	}
	if (args->redirect_stderr == EXEC_MKPIPE) {
		if (pipe2(stderr_pipe, O_CLOEXEC) < 0)
			goto exit;
		args->redirect_stderr = stderr_pipe[1];
	}

	if (args->redirect_stdin == EXEC_DEVNULL)
		args->redirect_stdin = devnull();
	if (args->redirect_stdout == EXEC_DEVNULL)
		args->redirect_stdout = devnull();
	if (args->redirect_stderr == EXEC_DEVNULL)
		args->redirect_stderr = devnull();

	sigfillset(&full);
	sigprocmask(SIG_SETMASK, &full, &old);
	res.pid = fork();

	if (res.pid < 0) {
		sigprocmask(SIG_SETMASK, &old, NULL);
		goto exit;
	} else if (res.pid == 0) {
		/* child */
		if (args->redirect_stdin != EXEC_NO_REDIRECT &&
		    dup2(args->redirect_stdin, STDIN_FILENO) < 0)
			goto child_err;
		if (args->redirect_stdout != EXEC_NO_REDIRECT &&
		    dup2(args->redirect_stdout, STDOUT_FILENO) < 0)
			goto child_err;
		if (args->redirect_stderr != EXEC_NO_REDIRECT &&
		    dup2(args->redirect_stderr, STDERR_FILENO) < 0)
			goto child_err;
		if (args->gid != (gid_t)-1 && setgid(args->gid) < 0)
			goto child_err;
		if (args->uid != (uid_t)-1 && setuid(args->uid) < 0)
			goto child_err;
		if (args->setsid && setsid() < 0)
			goto child_err;
		sigprocmask(SIG_SETMASK, &old, NULL);
		execvp(cmd, UNCONST(args->argv));
child_err:
		saved_errno = errno;
		write(execpipe[1], &saved_errno, sizeof saved_errno);
		_exit(1);
	} else {
		/* parent */
		close(execpipe[1]);
		execpipe[1] = -1;
		n = read(execpipe[0], &err, sizeof err);
		if (n != 0) {
			/* exec failed, reap the child and cleanup */
			waitpid(res.pid, NULL, 0); /* do we care about waitpid failures? */
			res.pid = -1;
		}
		sigprocmask(SIG_SETMASK, &old, NULL);
		if (res.pid < 0) {
			errno = (n == sizeof err) ? err : EINVAL;
			goto exit;
		}

		if (stdin_pipe[0] >= 0) {
			res.proc_stdin = stdin_pipe[1];
			stdin_pipe[1] = -1; /* to avoid closing it below */
		}
		if (stdout_pipe[0] >= 0) {
			res.proc_stdout = stdout_pipe[0];
			stdout_pipe[0] = -1;
		}
		if (stderr_pipe[0] >= 0) {
			res.proc_stderr = stderr_pipe[0];
			stderr_pipe[0] = -1;
		}
	}

exit:
	saved_errno = errno;
	checked_close(execpipe[0]); checked_close(execpipe[1]);
	checked_close(stdin_pipe[0]); checked_close(stdin_pipe[1]);
	checked_close(stdout_pipe[0]); checked_close(stdout_pipe[1]);
	checked_close(stderr_pipe[0]); checked_close(stderr_pipe[1]);
	errno = saved_errno;
	return res;
}

int rc_waitpid(pid_t pid)
{
	int status;
	while (waitpid(pid, &status, 0) == -1) {
		if (errno != EINTR) {
			status = -1;
			break;
		}
	}
	return status;
}

int rc_pipe_command(const char *cmd, int devnullfd)
{
	const char *argv[] = { "/bin/sh", "-c", cmd, NULL };
	struct exec_result res;
	struct exec_args args = exec_init(argv);
	args.redirect_stdin = EXEC_MKPIPE;
	args.redirect_stdout = args.redirect_stderr = devnullfd;
	res = do_exec(&args);
	return (res.pid > 0) ? res.proc_stdin : -1;
}
