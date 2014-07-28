/*
  checkpath.c
  Checks for the existance of a file or directory and creates it
  if necessary. It can also correct its ownership.
*/

/*
 * Copyright (c) 2007-2008 Roy Marples <roy@marples.name>
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

#include <sys/types.h>
#include <sys/stat.h>

#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <grp.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "builtins.h"
#include "einfo.h"
#include "rc-misc.h"

#ifdef HAVE_SELINUX
#include "rc-selinux.h"
#endif

typedef enum {
	inode_unknown = 0,
	inode_file = 1,
	inode_dir = 2,
	inode_fifo = 3,
} inode_t;

extern const char *applet;

static int do_check(char *path, uid_t uid, gid_t gid, mode_t mode,
	inode_t type, bool trunc, bool chowner, bool selinux_on)
{
	struct stat st;
	int fd, flags;
	int r;
	int u;

	memset(&st, 0, sizeof(st));
	if (stat(path, &st) || trunc) {
		if (type == inode_file) {
			einfo("%s: creating file", path);
			if (!mode) /* 664 */
				mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH;
			flags = O_CREAT|O_NDELAY|O_WRONLY|O_NOCTTY;
#ifdef O_CLOEXEC
			flags |= O_CLOEXEC;
#endif
#ifdef O_NOFOLLOW
			flags |= O_NOFOLLOW;
#endif
			if (trunc)
				flags |= O_TRUNC;
			u = umask(0);
			fd = open(path, flags, mode);
			umask(u);
			if (fd == -1) {
				eerror("%s: open: %s", applet, strerror(errno));
				return -1;
			}
			close (fd);
		} else if (type == inode_dir) {
			einfo("%s: creating directory", path);
			if (!mode) /* 775 */
				mode = S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH;
			u = umask(0);
			/* We do not recursively create parents */
			r = mkdir(path, mode);
			umask(u);
			if (r == -1 && errno != EEXIST) {
				eerror("%s: mkdir: %s", applet,
				    strerror (errno));
				return -1;
			}
			mode = 0;
		} else if (type == inode_fifo) {
			einfo("%s: creating fifo", path);
			if (!mode) /* 600 */
				mode = S_IRUSR | S_IWUSR;
			u = umask(0);
			r = mkfifo(path, mode);
			umask(u);
			if (r == -1 && errno != EEXIST) {
				eerror("%s: mkfifo: %s", applet,
				    strerror (errno));
				return -1;
			}
		}
	} else {
		if (type != inode_dir && S_ISDIR(st.st_mode)) {
			eerror("%s: is a directory", path);
			return 1;
		}
		if (type != inode_file && S_ISREG(st.st_mode)) {
			eerror("%s: is a file", path);
			return 1;
		}
		if (type != inode_fifo && S_ISFIFO(st.st_mode)) {
			eerror("%s: is a fifo", path);
			return -1;
		}
	}

	if (mode && (st.st_mode & 0777) != mode) {
		einfo("%s: correcting mode", path);
		if (chmod(path, mode)) {
			eerror("%s: chmod: %s", applet, strerror(errno));
			return -1;
		}
	}

	if (chowner && (st.st_uid != uid || st.st_gid != gid)) {
		einfo("%s: correcting owner", path);
		if (chown(path, uid, gid)) {
			eerror("%s: chown: %s", applet, strerror(errno));
			return -1;
		}
	}

#ifdef HAVE_SELINUX
	if (selinux_on)
		selinux_util_label(path);
#endif

	return 0;
}

static int parse_owner(struct passwd **user, struct group **group,
	const char *owner)
{
	char *u = xstrdup (owner);
	char *g = strchr (u, ':');
	int id = 0;
	int retval = 0;

	if (g)
		*g++ = '\0';

	if (user && *u) {
		if (sscanf(u, "%d", &id) == 1)
			*user = getpwuid((uid_t) id);
		else
			*user = getpwnam(u);
		if (*user == NULL)
			retval = -1;
	}

	if (group && g && *g) {
		if (sscanf(g, "%d", &id) == 1)
			*group = getgrgid((gid_t) id);
		else
			*group = getgrnam(g);
		if (*group == NULL)
			retval = -1;
	}

	free(u);
	return retval;
}

#include "_usage.h"
#define extraopts "path1 [path2] [...]"
#define getoptstring "dDfFpm:o:W" getoptstring_COMMON
static const struct option longopts[] = {
	{ "directory",          0, NULL, 'd'},
	{ "directory-truncate", 0, NULL, 'D'},
	{ "file",               0, NULL, 'f'},
	{ "file-truncate",      0, NULL, 'F'},
	{ "pipe",               0, NULL, 'p'},
	{ "mode",               1, NULL, 'm'},
	{ "owner",              1, NULL, 'o'},
	{ "writable",           0, NULL, 'W'},
	longopts_COMMON
};
static const char * const longopts_help[] = {
	"Create a directory if not exists",
	"Create/empty directory",
	"Create a file if not exists",
	"Truncate file",
	"Create a named pipe (FIFO) if not exists",
	"Mode to check",
	"Owner to check (user:group)",
	"Check whether the path is writable or not",
	longopts_help_COMMON
};
#include "_usage.c"

int checkpath(int argc, char **argv)
{
	int opt;
	uid_t uid = geteuid();
	gid_t gid = getgid();
	mode_t mode = 0;
	struct passwd *pw = NULL;
	struct group *gr = NULL;
	inode_t type = inode_unknown;
	int retval = EXIT_SUCCESS;
	bool trunc = false;
	bool chowner = false;
	bool writable = false;
	bool selinux_on = false;

	while ((opt = getopt_long(argc, argv, getoptstring,
		    longopts, (int *) 0)) != -1)
	{
		switch (opt) {
		case 'D':
			trunc = true;
		case 'd':
			type = inode_dir;
			break;
		case 'F':
			trunc = true;
		case 'f':
			type = inode_file;
			break;
		case 'p':
			type = inode_fifo;
			break;
		case 'm':
			if (parse_mode(&mode, optarg) != 0)
				eerrorx("%s: invalid mode `%s'",
				    applet, optarg);
			break;
		case 'o':
			chowner = true;
			if (parse_owner(&pw, &gr, optarg) != 0)
				eerrorx("%s: owner `%s' not found",
				    applet, optarg);
			break;
		case 'W':
			writable = true;
			break;

		case_RC_COMMON_GETOPT
		}
	}

	if (optind >= argc)
		usage(EXIT_FAILURE);

	if (writable && type != inode_unknown)
		eerrorx("%s: -W cannot be specified along with -d, -f or -p", applet);

	if (pw) {
		uid = pw->pw_uid;
		gid = pw->pw_gid;
	}
	if (gr)
		gid = gr->gr_gid;

#ifdef HAVE_SELINUX
	if (selinux_util_open() == 1)
		selinux_on = true;
#endif

	while (optind < argc) {
		if (writable)
			exit(!is_writable(argv[optind]));
		if (do_check(argv[optind], uid, gid, mode, type, trunc, chowner, selinux_on))
			retval = EXIT_FAILURE;
		optind++;
	}

#ifdef HAVE_SELINUX
	if (selinux_on)
		selinux_util_close();
#endif

	return retval;
}
