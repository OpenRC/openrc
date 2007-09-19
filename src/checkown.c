/*
   checkown.c
   Checks for the existance of a directory and creates it
   if necessary. It can also correct its ownership.

   Copyright 2007 Gentoo Foundation
   */

#define APPLET "checkown"

#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
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

static int do_check (char *path, uid_t uid, gid_t gid, mode_t mode)
{
	struct stat dirstat;

	memset (&dirstat, 0, sizeof (dirstat));

	if (stat (path, &dirstat)) {
		einfo ("%s: creating directory", path);
		if (! mode)
			mode = S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH;
		if (mkdir (path, mode)) {
			eerror ("%s: mkdir: %s", applet, strerror (errno));
			return (-1);
		}
	} else if (mode && (dirstat.st_mode & 0777) != mode) {
		einfo ("%s: correcting mode", applet);
		if (chmod (path, mode)) {
			eerror ("%s: chmod: %s", applet, strerror (errno));
			return (-1);
		}
	}

	if (dirstat.st_uid != uid || dirstat.st_gid != gid) {
		if (dirstat.st_dev || dirstat.st_ino)
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

	if (! pw)
		eerrorx ("%s: user `%s' not found", applet, token);

	*name = p;
	return (pw);
}

static struct group *get_group (const char *name)
{
	struct group *gr;
	int tid;

	if (sscanf (name, "%d", &tid) != 1)
		gr = getgrnam (name);
	else
		gr = getgrgid (tid);

	if (! gr)
		eerrorx ("%s: group `%s' not found", applet, name);

	return (gr);
}

#include "_usage.h"
#define getoptstring "m:g:u:" getoptstring_COMMON
static struct option longopts[] = {
	{ "mode",			1, NULL, 'm'},
	{ "user",           1, NULL, 'u'},
	{ "group",          1, NULL, 'g'},
	longopts_COMMON
	{ NULL,             0, NULL, 0}
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
	char *p;
	int retval = EXIT_SUCCESS;

	applet = argv[0];

	while ((opt = getopt_long (argc, argv, getoptstring,
							   longopts, (int *) 0)) != -1)
	{
		switch (opt) {
			case 'm':
				if (parse_mode (&mode, optarg))
					eerrorx ("%s: invalid mode `%s'", applet, optarg);
				break;
			case 'u':
				p = optarg;
				pw = get_user (&p);
				if (p && *p)
					optarg = p;
				else
					break;
			case 'g':
				gr = get_group (optarg);
				break;

				case_RC_COMMON_GETOPT
		}
	}

	if (pw) {
		uid = pw->pw_uid;
		gid = pw->pw_gid;
	}
	if (gr)
		gid = gr->gr_gid;

	while (optind < argc) {
		if (do_check (argv[optind], uid, gid, mode))
			retval = EXIT_FAILURE;
		optind++;
	}

	exit (retval);
}
