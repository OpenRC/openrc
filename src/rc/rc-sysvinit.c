/*
 * rc-sysvinit.c
 * Helper to send a runlevel change to sysvinit
 */

/*
 * Copyright (c) 2019 The OpenRC Authors.
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
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "einfo.h"
#include "rc-sysvinit.h"

void sysvinit_runlevel(char rl)
{
	struct init_request request;
	int fd;
	char *p;
	size_t bytes;
	ssize_t r;

	if (!rl)
		return;

	fd = open("/run/initctl", O_WRONLY|O_NONBLOCK|O_CLOEXEC|O_NOCTTY);
	if (fd < 0) {
		if (errno != ENOENT)
			eerror("Failed to open initctl fifo: %s", strerror(errno));
		return;
	}
	request = (struct init_request) {
		.magic = INIT_MAGIC,
		.sleeptime = 0,
		.cmd = INIT_CMD_RUNLVL,
		.runlevel = rl,
	};
	p = (char *) &request;
	bytes = sizeof(request);
	do {
		r = write(fd, p, bytes);
		if (r < 0) {
			if ((errno == EAGAIN) || (errno == EINTR))
				continue;
			eerror("Failed to write to /run/initctl: %s", strerror(errno));
			return;
		}
		p += r;
		bytes -= r;
	} while (bytes > 0);
	exit(0);
}
