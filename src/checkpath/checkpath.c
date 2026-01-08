/*
 * checkpath.c
 * Checks for the existence of a file or directory and creates it
 * if necessary. It can also correct its ownership.
 */

/*
 * Copyright (c) 2007-2015 The OpenRC Authors.
 * See the Authors file at the top-level directory of this distribution and
 * https://github.com/OpenRC/openrc/blob/HEAD/AUTHORS
 *
 * This file is part of OpenRC. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution and at https://github.com/OpenRC/openrc/blob/HEAD/LICENSE
 * This file may not be copied, modified, propagated, or distributed
 *    except according to the terms contained in the LICENSE file.
 */

#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <grp.h>
#include <pwd.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "einfo.h"
#include "misc.h"
#include "selinux.h"
#include "_usage.h"
#include "helpers.h"

typedef enum {
	inode_unknown = 0,
	inode_file = 1,
	inode_dir = 2,
	inode_fifo = 3,
} inode_t;

#define opts(opt, opt_long) \
	opt(d, directory, no_argument, "Create a directory if not exists") \
	opt(D, directory-truncate, no_argument, "Create/empty directory") \
	opt(f, file, no_argument, "Create a file if not exists") \
	opt(F, file-truncate, no_argument, "Truncate file") \
	opt(p, pipe, no_argument, "Create a named pipe (FIFO) if not exists") \
	opt(m, mode, required_argument, "Mode to check") \
	opt(o, owner, required_argument, "Owner to check (user:group)") \
	opt(s, symlinks, no_argument, "follow symbolic links (irrelevant on linux)") \
	opt(W, writable, no_argument, "Check whether the path is writable or not")
cmdline_opts(opts)

const char *applet = NULL;
const char *extraopts ="path1 [path2] [...]";
const char *usagestring = NULL;

static int get_dirfd(char *path, bool symlinks)
{
	char *ch, *item, *path_dupe, *str, *linkpath = NULL;
	int components = 0, dirfd, flags = 0, new_dirfd;
	ssize_t linksize;
	struct stat st;

	if (!path || *path != '/')
		eerrorx("%s: empty or relative path", applet);
	dirfd = openat(AT_FDCWD, "/", O_RDONLY);
	if (dirfd == -1)
		eerrorx("%s: unable to open the root directory: %s", applet, strerror(errno));
	ch = path;
	while (*ch) {
		if (*ch == '/')
			components++;
		ch++;
	}
	path_dupe = xstrdup(path);
	item = strtok(path_dupe, "/");
#ifdef O_PATH
	flags |= O_PATH;
#endif
	if (!symlinks)
		flags |= O_NOFOLLOW;
	flags |= O_RDONLY;
	while (dirfd > 0 && item && components > 1) {
		str = xstrdup(linkpath ? linkpath : item);
		new_dirfd = openat(dirfd, str, flags);
		if (new_dirfd == -1)
			eerrorx("%s: %s: could not open %s: %s", applet, path, str, strerror(errno));
		if (fstat(new_dirfd, &st) == -1)
			eerrorx("%s: %s: unable to stat %s: %s", applet, path, item, strerror(errno));
		if (S_ISLNK(st.st_mode) ) {
			if (st.st_uid != 0)
				eerrorx("%s: %s: symbolic link %s not owned by root", applet, path, str);
			linksize = st.st_size+1;
			if (linkpath)
				free(linkpath);
			linkpath = xmalloc(linksize);
			memset(linkpath, 0, linksize);
			if (readlinkat(new_dirfd, "", linkpath, linksize) != st.st_size)
				eerrorx("%s: symbolic link destination changed", applet);
			/*
			 * now follow the symlink.
			 */
			close(new_dirfd);
		} else {
			/* now walk down the directory path */
			close(dirfd);
			dirfd = new_dirfd;
			free(linkpath);
			linkpath = NULL;
			item = strtok(NULL, "/");
			components--;
		}

		free(str);
	}
	free(path_dupe);
	free(linkpath);
	return dirfd;
}

static char *clean_path(char *path)
{
	char *ch, *ch2, *str;

	str = xmalloc(strlen(path) + 1);
	ch = path;
	ch2 = str;
	while (true) {
		*ch2 = *ch;
		ch++;
		ch2++;
		if (!*(ch-1))
			break;
		while (*(ch - 1) == '/' && *ch == '/')
			ch++;
	}
	/* get rid of trailing / characters */
	while ((ch = strrchr(str, '/'))) {
		if (ch == str)
			break;
		if (!*(ch+1))
			*ch = 0;
		else
			break;
	}
	return str;
}

static size_t masks[] = {
	[inode_unknown] = 0,
	[inode_file] = S_IFREG,
	[inode_dir] = S_IFDIR,
	[inode_fifo] = S_IFIFO,
};

static int do_create(inode_t type, const char *path, int dirfd, const char *name, int flags, mode_t mode)
{
	int mask = umask(0);
	int fd;

	static const mode_t default_mode[] = {
		[inode_file] = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH, /* 664 */
		[inode_dir] = S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH, /* 775 */
		[inode_fifo] = S_IRUSR | S_IWUSR, /* 600 */
		[inode_unknown] = 0,
	};

	if (!mode)
		mode = default_mode[type];

	switch (type) {
	case inode_file:
		einfo("%s: creating file", path);
		flags |= O_CREAT;
		break;
	case inode_dir:
		einfo("%s: creating directory", path);

		/* We do not recursively create parents */
		if (mkdirat(dirfd, name, mode) == -1 && errno != EEXIST) {
			eerror("%s: mkdirat: %s", applet, strerror (errno));
			return -1;
		}

		break;
	case inode_fifo:
		einfo("%s: creating fifo", path);

		if (mkfifoat(dirfd, name, mode) == -1 && errno != EEXIST) {
			eerror("%s: mkfifo: %s", applet, strerror (errno));
			return -1;
		}

		break;
	case inode_unknown:
		break;
	}
	umask(mask);

	if ((fd = openat(dirfd, name, flags, mode)) == -1) {
		eerror("%s: open: %s", applet, strerror(errno));
		return -1;
	}
	return fd;
}

static int do_check(char *path, uid_t uid, gid_t gid, mode_t mode, inode_t type,
		bool trunc, bool chowner, bool writable, bool symlinks, bool selinux_on)
{
	int flags = O_NDELAY | O_NOCTTY | O_RDONLY | O_CLOEXEC | O_NOFOLLOW | (trunc ? O_TRUNC : 0);
	const char *name = basename_c(path);
	struct stat st;
	int dirfd, fd;

	if (writable) {
		if (access(path, W_OK) == 0)
			return 0;
		if (errno != ENOENT || type == inode_unknown)
			return -1;
	}

	dirfd = get_dirfd(path, symlinks);
	if ((fd = openat(dirfd, name, flags)) == -1 &&
			(fd = do_create(type, path, dirfd, name, flags, mode)) == -1)
		return -1;

	if (fstat(fd, &st) == -1) {
		eerror("fstat: %s: %s", path, strerror(errno));
		goto err;
	}

	if (type != inode_dir && S_ISDIR(st.st_mode)) {
		eerror("%s: is a directory", path);
		goto err;
	}

	if (type != inode_file && S_ISREG(st.st_mode)) {
		eerror("%s: is a file", path);
		goto err;
	}

	if (type != inode_fifo && S_ISFIFO(st.st_mode)) {
		eerror("%s: is a fifo", path);
		goto err;
	}

	if (mode && (st.st_mode & 07777) != mode) {
		if ((type != inode_dir) && (st.st_nlink > 1)) {
			eerror("%s: chmod: Too many hard links to %s", applet, path);
			goto err;
		}

		if (S_ISLNK(st.st_mode)) {
			eerror("%s: chmod: %s is a symbolic link", applet, path);
			goto err;
		}

		einfo("%s: correcting mode", path);
		if (fchmod(fd, mode)) {
			eerror("%s: chmod: %s", applet, strerror(errno));
			goto err;
		}
	}

	if (chowner && (st.st_uid != uid || st.st_gid != gid)) {
		if ((type != inode_dir) && (st.st_nlink > 1)) {
			eerror("%s: chown: Too many hardlinks to %s", applet, path);
			goto err;
		}

		if (S_ISLNK(st.st_mode)) {
			eerror("%s: chown: %s is a symbolic link", applet, path);
			goto err;
		}

		einfo("%s: correcting owner", path);
		if (fchown(fd, uid, gid)) {
			eerror("%s: chown: %s", applet, strerror(errno));
			goto err;
		}
	}

	if (selinux_on)
		selinux_util_label(path);
	close(fd);

	return 0;
err:
	close(fd);
	return -1;
}

static int parse_owner(struct passwd **user, struct group **group, const char *owner)
{
	char *u = xstrdup(owner), *g = strchr(u, ':');
	int id = 0, retval = 0;

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

int main(int argc, char **argv)
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
	bool symlinks = false;
	bool writable = false;
	bool selinux_on = false;
	char *path = NULL;

	applet = basename_c(argv[0]);
	while ((opt = getopt_long(argc, argv, getoptstring, longopts, (int *) 0)) != -1) {
		switch (opt) {
		case 'D':
			trunc = true;
			/* falls through */
		case 'd':
			type = inode_dir;
			break;
		case 'F':
			trunc = true;
			/* falls through */
		case 'f':
			type = inode_file;
			break;
		case 'p':
			type = inode_fifo;
			break;
		case 'm':
			if (parse_mode(&mode, optarg) != 0)
				eerrorx("%s: invalid mode '%s'", applet, optarg);
			break;
		case 'o':
			chowner = true;
			if (parse_owner(&pw, &gr, optarg) != 0)
				eerrorx("%s: owner '%s' not found", applet, optarg);
			break;
		case 's':
#ifndef O_PATH
			symlinks = true;
#endif
			break;
		case 'W':
			writable = true;
			break;

		case_RC_COMMON_GETOPT
		}
	}

	if (optind >= argc)
		usage(EXIT_FAILURE);

	if (pw) {
		uid = pw->pw_uid;
		gid = pw->pw_gid;
	}
	if (gr)
		gid = gr->gr_gid;

	if (selinux_util_open() == 1)
		selinux_on = true;

	while (optind < argc) {
		path = clean_path(argv[optind]);
		if (do_check(path, uid, gid, mode, type, trunc, chowner, writable, symlinks, selinux_on))
			retval = EXIT_FAILURE;
		optind++;
		free(path);
	}

	if (selinux_on)
		selinux_util_close();

	return retval;
}
