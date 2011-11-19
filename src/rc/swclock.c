/*
  swclock.c
  Sets the system time from the mtime of the given file.
  This is useful for systems who do not have a working RTC and rely on ntp.
  OpenRC relies on the correct system time for a lot of operations so this is needed
  quite early.
*/

/*
 * Copyright (c) 2009 Roy Marples <roy@marples.name>
 *
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

#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <unistd.h>
#include <utime.h>

#include "builtins.h"
#include "einfo.h"
#include "rc-misc.h"

#define RC_SHUTDOWNTIME    RC_SVCDIR "/shutdowntime"

extern const char *applet;

#include "_usage.h"
#define extraopts "file"
#define getoptstring "sw" getoptstring_COMMON
static const struct option longopts[] = {
	{ "save", 0, NULL, 's' },
	{ "warn", 0, NULL, 'w' },
	longopts_COMMON
};
static const char * const longopts_help[] = {
	"saves the time",
	"no error if no reference file",
	longopts_help_COMMON
};
#include "_usage.c"

int
swclock(int argc, char **argv)
{
	int opt, sflag = 0, wflag = 0;
	const char *file = RC_SHUTDOWNTIME;
	struct stat sb;
	struct timeval tv;

	while ((opt = getopt_long(argc, argv, getoptstring,
		    longopts, (int *) 0)) != -1)
	{
		switch (opt) {
		case 's':
			sflag = 1;
			break;
		case 'w':
			wflag = 1;
			break;
		case_RC_COMMON_GETOPT
		}
	}

	if (optind < argc)
		file = argv[optind++];

	if (sflag) {
		if (stat(file, &sb) == -1) {
			opt = open(file, O_WRONLY | O_CREAT, 0644);
			if (opt == -1)
				eerrorx("swclock: open: %s", strerror(errno));
			close(opt);
		} else
			if (utime(file, NULL) == -1)
				eerrorx("swclock: utime: %s", strerror(errno));
		return 0;
	}

	if (stat(file, &sb) == -1) {
		if (wflag != 0 && errno == ENOENT)
			ewarn("swclock: `%s': %s", file, strerror(errno));
		else
			eerrorx("swclock: `%s': %s", file, strerror(errno));
		return 0;
	}

	tv.tv_sec = sb.st_mtime;
	tv.tv_usec = 0;

	if (settimeofday(&tv, NULL) == -1)
		eerrorx("swclock: settimeofday: %s", strerror(errno));

	return 0;
}
