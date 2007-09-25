/*
   fstabinfo.c
   Gets information about /etc/fstab.

   Copyright 2007 Gentoo Foundation
   */

#define APPLET "fstabinfo"

#include <errno.h>
#include <getopt.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Yay for linux and it's non liking of POSIX functions.
   Okay, we could use getfsent but the man page says use getmntent instead
   AND we don't have getfsent on uclibc or dietlibc for some odd reason. */
#ifdef __linux__
#define HAVE_GETMNTENT
#include <mntent.h>
#define GET_ENT getmntent (fp)
#define GET_ENT_FILE(_name) getmntfile (fp, _name)
#define END_ENT endmntent (fp)
#define ENT_DEVICE(_ent) ent->mnt_fsname
#define ENT_FILE(_ent) ent->mnt_dir
#define ENT_TYPE(_ent) ent->mnt_type
#define ENT_OPTS(_ent) ent->mnt_opts
#define ENT_PASS(_ent) ent->mnt_passno
#else
#define HAVE_GETFSENT
#include <fstab.h>
#define GET_ENT getfsent ()
#define GET_ENT_FILE(_name) getfsfile (_name)
#define END_ENT endfsent ()
#define ENT_DEVICE(_ent) ent->fs_spec
#define ENT_TYPE(_ent) ent->fs_vfstype
#define ENT_FILE(_ent) ent->fs_file
#define ENT_OPTS(_ent) ent->fs_mntops
#define ENT_PASS(_ent) ent->fs_passno
#endif

#include "builtins.h"
#include "einfo.h"

#ifdef HAVE_GETMNTENT
static struct mntent *getmntfile (FILE *fp, const char *file)
{
	struct mntent *ent;

	while ((ent = getmntent (fp)))
		if (strcmp (file, ent->mnt_dir) == 0)
			return (ent);

	return (NULL);
}
#endif

#include "_usage.h"
#define getoptstring "m:o:p:t:" getoptstring_COMMON
static struct option longopts[] = {
	{ "mountcmd",       1, NULL, 'm'},
	{ "options",        1, NULL, 'o'},
	{ "passno",         1, NULL, 'p'},
	{ "fstype",         1, NULL, 't'},
	longopts_COMMON
	{ NULL,             0, NULL, 0}
};
#include "_usage.c"

int fstabinfo (int argc, char **argv)
{
#ifdef HAVE_GETMNTENT
	FILE *fp;
	struct mntent *ent;
#else
	struct fstab *ent;
#endif
	int result = EXIT_FAILURE;
	char *token;
	int n = 0;
	int opt;

	while ((opt = getopt_long (argc, argv, getoptstring,
							   longopts, (int *) 0)) != -1)
	{
#ifdef HAVE_GETMNTENT
		fp = setmntent ("/etc/fstab", "r");
#endif
		switch (opt) {
			case 'm':
				if ((ent = GET_ENT_FILE (optarg))) {
					printf ("-o %s -t %s %s %s\n", ENT_OPTS (ent), ENT_TYPE (ent),
							ENT_DEVICE (ent), ENT_FILE (ent));
					result = EXIT_SUCCESS;
				}
				break;

			case 'o':
				if ((ent = GET_ENT_FILE (optarg))) {
					printf ("%s\n", ENT_OPTS (ent));
					result = EXIT_SUCCESS;
				}
				break;

			case 'p':
				switch (optarg[0]) {
					case '=':
					case '<':
					case '>':
						if (sscanf (optarg + 1, "%d", &n) != 1)
							eerrorx ("%s: invalid passno %s", argv[0], optarg + 1);

						while ((ent = GET_ENT)) {
							if (((optarg[0] == '=' && n == ENT_PASS (ent)) ||
								 (optarg[0] == '<' && n > ENT_PASS (ent)) ||
								 (optarg[0] == '>' && n < ENT_PASS (ent))) &&
								strcmp (ENT_FILE (ent), "none") != 0)
							{
								printf ("%s\n", ENT_FILE (ent));
								result = EXIT_SUCCESS;
							}
						}
						break;

					default:
						if ((ent = GET_ENT_FILE (optarg))) {
							printf ("%d\n", ENT_PASS (ent));
							result = EXIT_SUCCESS;
						}
						break;
				}
				break;

			case 't':
				while ((token = strsep (&optarg, ",")))
					while ((ent = GET_ENT))
						if (strcmp (token, ENT_TYPE (ent)) == 0)
							printf ("%s\n", ENT_FILE (ent));
				result = EXIT_SUCCESS;
				break;

			case_RC_COMMON_GETOPT
		}

		END_ENT;

		if (result != EXIT_SUCCESS)
			break;
	}

	if (result != EXIT_SUCCESS && argc == optind)
		eerrorx ("%s: no arguments specified", argv[0]);

	exit (result);
}
