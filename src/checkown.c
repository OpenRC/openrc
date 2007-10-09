/*
   checkown.c
   Checks for the existance of a file or directory and creates it
   if necessary. It can also correct its ownership.

   Copyright 2007 Gentoo Foundation
   */

#define APPLET "checkown"

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

static char *applet = NULL;

static int do_check (char *path, uid_t uid, gid_t gid, mode_t mode, int file)
{
	struct stat st;

	memset (&st, 0, sizeof (struct stat));

	if (stat (path, &st)) {
		if (file) {
			int fd;
			einfo ("%s: creating file", path);
			if ((fd = open (path, O_CREAT)) == -1) {
				eerror ("%s: open: %s", applet, strerror (errno));
				return (-1);
			}
			close (fd);	
		} else {
			einfo ("%s: creating directory", path);
			if (! mode)
				mode = S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH;
			if (mkdir (path, mode)) {
				eerror ("%s: mkdir: %s", applet, strerror (errno));
				return (-1);
			}
			mode = 0;
		}
	} else {
		if ((file && S_ISDIR (st.st_mode)) ||
			(! file && ! S_ISDIR (st.st_mode)))
		{
			if (file)
				eerror ("%s: is a directory", path);
			else
				eerror ("%s: is a file", path);
			return (-1);
		}
	}
	
	if (mode && (st.st_mode & 0777) != mode) {
		einfo ("%s: correcting mode", applet);
		if (chmod (path, mode)) {
			eerror ("%s: chmod: %s", applet, strerror (errno));
			return (-1);
		}
	}

	if (st.st_uid != uid || st.st_gid != gid) {
		if (st.st_dev || st.st_ino)
			einfo ("%s: correcting owner", path);
		if (chown (path, uid, gid)) {
			eerror ("%s: chown: %s", applet, strerror (errno));
			return (-1);
		}
	}

	return (0);
}

/* Based on busybox */
static int parse_mode (mode_t *mode, char *text)
{
	/* Check for a numeric mode */
	if ((*mode - '0') < 8) {
		char *p;
		unsigned long l = strtoul (text, &p, 8);
		if (*p || l > 07777U) {
			errno = EINVAL;
			return (-1);
		}
		*mode = l;
		return (0);
	}

	/* We currently don't check g+w type stuff */
	errno = EINVAL;
	return (-1);
}

static struct passwd *get_user (char **name)
{
	struct passwd *pw;
	char *p = *name;
	char *token;
	int tid;

	token = strsep (&p, ":");
	if (sscanf (token, "%d", &tid) != 1)
		pw = getpwnam (token);
	else
		pw = getpwuid (tid);

	if (pw)
		*name = p;

	return (pw);
}

static struct group *get_group (const char *name)
{
	int tid;

	if (sscanf (name, "%d", &tid) != 1)
		return (getgrnam (name));
	else
		return (getgrgid (tid));
}

#include "_usage.h"
#define extraopts "dir1 dir2 ..."
#define getoptstring "fm:g:u:" getoptstring_COMMON
static struct option longopts[] = {
	{ "directory",      0, NULL, 'd'},
	{ "file",           0, NULL, 'f'},
	{ "mode",			1, NULL, 'm'},
	{ "user",           1, NULL, 'u'},
	{ "group",          1, NULL, 'g'},
	longopts_COMMON
};
static const char * const longopts_help[] = {
	"Check if a directory",
	"Check if a file",
	"Mode to check",
	"User to check",
	"Group to check",
	longopts_help_COMMON
};
#include "_usage.c"

int checkown (int argc, char **argv)
{
	int opt;
	uid_t uid = geteuid();
	gid_t gid = getgid();
	mode_t mode = 0;
	struct passwd *pw = NULL;
	struct group *gr = NULL;
	bool file = 0;

	char *p;
	int retval = EXIT_SUCCESS;

	applet = argv[0];

	while ((opt = getopt_long (argc, argv, getoptstring,
							   longopts, (int *) 0)) != -1)
	{
		switch (opt) {
			case 'd':
				file = 0;
				break;
			case 'f':
				file = 1;
				break;
			case 'm':
				if (parse_mode (&mode, optarg))
					eerrorx ("%s: invalid mode `%s'", applet, optarg);
				break;
			case 'u':
				p = optarg;
				if (! (pw = get_user (&p)))
					eerrorx ("%s: user `%s' not found", applet, optarg);
				if (p && *p)
					optarg = p;
				else
					break;
			case 'g':
				if (! (gr = get_group (optarg)))
					eerrorx ("%s: group `%s' not found", applet, optarg);
				break;

				case_RC_COMMON_GETOPT
		}
	}

	if (optind >= argc)
		usage (EXIT_FAILURE);

	if (pw) {
		uid = pw->pw_uid;
		gid = pw->pw_gid;
	}
	if (gr)
		gid = gr->gr_gid;

	while (optind < argc) {
		if (do_check (argv[optind], uid, gid, mode, file))
			retval = EXIT_FAILURE;
		optind++;
	}

	exit (retval);
}
