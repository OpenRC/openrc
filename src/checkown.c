/*
   checkown.c
   Checks for the existance of a file or directory and creates it
   if necessary. It can also correct its ownership.
   */

/* 
 * Copyright 2007 Roy Marples
 * All rights reserved

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

static const char *applet;

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

static int parse_owner (struct passwd **user, struct group **group,
						const char *owner)
{
	char *u = xstrdup (owner);
	char *g = strchr (u, ':');
	int id = 0;
	int retval = 0;

	if (g)
		*g++ = '\0';

	if (user && *u) {
		if (sscanf (u, "%d", &id) == 1)
			*user = getpwuid (id);
		else	
			*user = getpwnam (u);
		if (! *user)
			retval = -1;
	}

	if (group && g && *g) {
		if (sscanf (g, "%d", &id) == 1)
			*group = getgrgid (id);
		else
			*group = getgrnam (g);
		if (! *group)
			retval = -1;
	}

	free (u);
	return (retval);
}

#include "_usage.h"
#define extraopts "dir1 dir2 ..."
#define getoptstring "fm:o:" getoptstring_COMMON
static struct option longopts[] = {
	{ "directory",      0, NULL, 'd'},
	{ "file",           0, NULL, 'f'},
	{ "mode",			1, NULL, 'm'},
	{ "owner",          1, NULL, 'o'},
	longopts_COMMON
};
static const char * const longopts_help[] = {
	"Check if a directory",
	"Check if a file",
	"Mode to check",
	"Owner to check (user:group)",
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

	applet = basename_c (argv[0]);
	int retval = EXIT_SUCCESS;

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
				if (parse_mode (&mode, optarg) != 0)
					eerrorx ("%s: invalid mode `%s'", applet, optarg);
				break;
			case 'o':
				if (parse_owner (&pw, &gr, optarg) != 0)
					eerrorx ("%s: owner `%s' not found", applet, optarg);
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
