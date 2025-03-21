/*
 * pipes.c
 * Helper to handle spawning processes and connecting them to pipes.
 */

/*
 * Copyright (c) 2018 The OpenRC Authors.
 * See the Authors file at the top-level directory of this distribution and
 * https://github.com/OpenRC/openrc/blob/HEAD/AUTHORS
 *
 * This file is part of OpenRC. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution and at https://github.com/OpenRC/openrc/blob/HEAD/LICENSE
 * This file may not be copied, modified, propagated, or distributed
 *    except according to the terms contained in the LICENSE file.
 */

#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

#include "pipes.h"

static const int pipe_read_end = 0;
static const int pipe_write_end = 1;

/*
 * Starts a shell command with stdin redirected from a pipe and
 * std{out,err} redirected to /dev/null (provided by the caller).
 * Returns the write end of the pipe or -1
 */
int rc_pipe_command(char *cmd, int devnullfd)
{
	int pfd[2];
	pid_t pid;

	if (pipe(pfd) < 0)
		return -1;

	pid = fork();
	if (pid > 0) {
		/* parent */
		close(pfd[pipe_read_end]);
		return pfd[pipe_write_end];
	} else if (pid == 0) {
		/* child */
		close(pfd[pipe_write_end]);
		if (pfd[pipe_read_end] != STDIN_FILENO) {
			if (dup2(pfd[pipe_read_end], STDIN_FILENO) < 0)
				_exit(1);
			close(pfd[pipe_read_end]);
		}
		dup2(devnullfd, STDOUT_FILENO);
		dup2(devnullfd, STDERR_FILENO);
		execl("/bin/sh", "sh", "-c", cmd, NULL);
		_exit(1);
	} else {
		/* error */
		close(pfd[pipe_read_end]);
		close(pfd[pipe_write_end]);
	}
	return -1;
}
