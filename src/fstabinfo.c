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
#define START_ENT fp = setmntent ("/etc/fstab", "r");
#define GET_ENT getmntent (fp)
#define GET_ENT_FILE(_name) getmntfile (_name)
#define END_ENT endmntent (fp)
#define ENT_DEVICE(_ent) ent->mnt_fsname
#define ENT_FILE(_ent) ent->mnt_dir
#define ENT_TYPE(_ent) ent->mnt_type
#define ENT_OPTS(_ent) ent->mnt_opts
#define ENT_PASS(_ent) ent->mnt_passno
#else
#define HAVE_GETFSENT
#include <fstab.h>
#define START_ENT
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
#include "rc.h"
#include "strlist.h"

#ifdef HAVE_GETMNTENT
static struct mntent *getmntfile (const char *file)
{
	struct mntent *ent = NULL;
	FILE *fp;

	START_ENT;
	while ((ent = getmntent (fp)))
		if (strcmp (file, ent->mnt_dir) == 0)
			break;
	END_ENT;
	
	return (ent);
}
#endif

#include "_usage.h"
#define getoptstring "mop:t:" getoptstring_COMMON
static struct option longopts[] = {
	{ "options",        0, NULL, 'o'},
	{ "passno",         1, NULL, 'p'},
	{ "fstype",         1, NULL, 't'},
	longopts_COMMON
	{ NULL,             0, NULL, 0}
};
static const char * const longopts_help[] = {
	"Construct the arguments to give to mount",
	"Extract the options field",
	"Extract the pass number field",
	"Extract the file system type",
	longopts_help_COMMON
};
#include "_usage.c"

#define OUTPUT_FILE      (1 << 1)
#define OUTPUT_OPTIONS   (1 << 3)
#define OUTPUT_PASSNO    (1 << 4)

int fstabinfo (int argc, char **argv)
{
#ifdef HAVE_GETMNTENT
	FILE *fp;
	struct mntent *ent;
#else
	struct fstab *ent;
#endif
	int result = EXIT_SUCCESS;
	char *token;
	int i;
	int opt;
	int output = OUTPUT_FILE;
	char **files = NULL;
	char *file;
	bool filtered = false;

	while ((opt = getopt_long (argc, argv, getoptstring,
							   longopts, (int *) 0)) != -1)
	{
		switch (opt) {
			case 'o':
				output = OUTPUT_OPTIONS;
				break;

			case 'p':
				switch (optarg[0]) {
					case '=':
					case '<':
					case '>':
						if (sscanf (optarg + 1, "%d", &i) != 1)
							eerrorx ("%s: invalid passno %s", argv[0], optarg + 1);

						filtered = true;
						START_ENT;
						while ((ent = GET_ENT)) {
							if (((optarg[0] == '=' && i == ENT_PASS (ent)) ||
								 (optarg[0] == '<' && i > ENT_PASS (ent)) ||
								 (optarg[0] == '>' && i < ENT_PASS (ent))) &&
								strcmp (ENT_FILE (ent), "none") != 0)
								rc_strlist_add (&files, ENT_FILE (ent));
						}
						END_ENT;
						break;

					default:
						rc_strlist_add (&files, optarg);
						output = OUTPUT_PASSNO;
						break;
				}
				break;

			case 't':
				filtered = true;
				while ((token = strsep (&optarg, ","))) {
					START_ENT;
					while ((ent = GET_ENT))
						if (strcmp (token, ENT_TYPE (ent)) == 0)
							rc_strlist_add (&files, ENT_FILE (ent));
					END_ENT;
				}
				break;

			case_RC_COMMON_GETOPT
		}
	}

	while (optind < argc)
		rc_strlist_add (&files, argv[optind++]);

	if (! files && ! filtered) {
		START_ENT;
		while ((ent = GET_ENT))
			rc_strlist_add (&files, ENT_FILE (ent));
		END_ENT;

		if (! files)
			eerrorx ("%s: emtpy fstab", argv[0]);
	}

	/* Ensure we always display something */
	START_ENT;
	STRLIST_FOREACH (files, file, i) {
		if (! (ent = GET_ENT_FILE (file))) {
			result = EXIT_FAILURE;
			continue;
		}

		/* No point in outputting if quiet */
		if (rc_env_bool ("RC_QUIET"))
			continue;

		switch (output) {
			case OUTPUT_OPTIONS:
				printf ("%s\n", ENT_OPTS (ent));
				break;

			case OUTPUT_FILE:
				printf ("%s\n", file);
				break;

			case OUTPUT_PASSNO:
				printf ("%d\n", ENT_PASS (ent));
				break;
		}
	}
	END_ENT;

	rc_strlist_free (files);
	exit (result);
}
