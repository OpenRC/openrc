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

static void sysvinit_send_cmd(struct init_request *request)
{
	int fd;
	char *p;
	size_t bytes;
	ssize_t r;

	fd = open("/run/initctl", O_WRONLY|O_NONBLOCK|O_CLOEXEC|O_NOCTTY);
	if (fd < 0) {
		if (errno != ENOENT)
			eerror("Failed to open initctl fifo: %s", strerror(errno));
		return;
	}
	p = (char *) request;
	bytes = sizeof(*request);
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
}

void sysvinit_runlevel(char rl)
{
	struct init_request request;

	if (!rl)
		return;

	request = (struct init_request) {
		.magic = INIT_MAGIC,
		.sleeptime = 0,
		.cmd = INIT_CMD_RUNLVL,
		.runlevel = rl,
	};
	sysvinit_send_cmd(&request);
		return;
}

/*
 *	Set environment variables in the init process.
 */
void sysvinit_setenv(const char *name, const char *value)
{
	struct init_request	request;
	size_t nl;
	size_t vl;

	memset(&request, 0, sizeof(request));
	request.magic = INIT_MAGIC;
	request.cmd = INIT_CMD_SETENV;
	nl = strlen(name);
	if (value)
		vl = strlen(value);
else
		vl = 0;

	if (nl + vl + 3 >= (int)sizeof(request.i.data))
		return;

	memcpy(request.i.data, name, nl);
	if (value) {
		request.i.data[nl] = '=';
		memcpy(request.i.data + nl + 1, value, vl);
	}
	sysvinit_send_cmd(&request);
	return;
}
