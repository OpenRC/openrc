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
#ifndef RC_EXEC_H
#define RC_EXEC_H

#include <stdbool.h>
#include <sys/types.h>

/*
 * goal of this api is to provide a convenient and centralized way to do most
 * of the fork() + exec() work. it doesn't attempt to do everything, but simple
 * use-cases should be covered. if any simple functionality is missing, add it
 * here instead of ad-hoc coding fork+exec yourself.
 */

enum {
	/* default */
	RC_EXEC_REDIRECT_NONE   = -1,
	/* make a pipe and redirect to it */
	RC_EXEC_MKPIPE          = -2,
};

struct rc_exec_args {
	const char **argv;
	int redirect_stdin;
	int redirect_stdout;
	int redirect_stderr;
	bool setsid : 1;
};

struct rc_exec_result {
	pid_t pid;
	/* returned in case of RC_EXEC_MKPIPE */
	int proc_stdin;
	int proc_stdout;
	int proc_stderr;
};

struct rc_exec_args rc_exec_args_init(const char **argv);
struct rc_exec_result rc_exec(struct rc_exec_args *args);

#endif
