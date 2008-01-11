/*
   fstabinfo.c
   Gets information about /etc/fstab.
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

#include <errno.h>
#include <getopt.h>
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
#define ENT_BLOCKDEVICE(_ent) ent->mnt_fsname
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
#define ENT_BLOCKDEVICE(_ent) ent->fs_spec
#define ENT_TYPE(_ent) ent->fs_vfstype
#define ENT_FILE(_ent) ent->fs_file
#define ENT_OPTS(_ent) ent->fs_mntops
#define ENT_PASS(_ent) ent->fs_passno
#endif

#include "builtins.h"
#include "einfo.h"
#include "rc.h"
#include "rc-misc.h"
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

static const char *applet = NULL;

#include "_usage.h"
#define getoptstring "bmop:t:" getoptstring_COMMON
static struct option longopts[] = {
	{ "blockdevice",    0, NULL, 'b' },
	{ "options",        0, NULL, 'o' },
	{ "passno",         1, NULL, 'p' },
	{ "fstype",         1, NULL, 't' },
	longopts_COMMON
};
static const char * const longopts_help[] = {
	"Extract the block device",
	"Extract the options field",
	"Extract or query the pass number field",
	"List entries with matching file system type",
	longopts_help_COMMON
};
#include "_usage.c"

#define OUTPUT_FILE      (1 << 1)
#define OUTPUT_OPTIONS   (1 << 3)
#define OUTPUT_PASSNO    (1 << 4)
#define OUTPUT_BLOCKDEV  (1 << 5)

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

	applet = basename_c (argv[0]); 

	/* Ensure that we are only quiet when explicitly told to be */
	unsetenv ("EINFO_QUIET");

	while ((opt = getopt_long (argc, argv, getoptstring,
				   longopts, (int *) 0)) != -1)
	{
		switch (opt) {
			case 'b':
				output = OUTPUT_BLOCKDEV;
				break;
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
		if (rc_yesno (getenv ("EINFO_QUIET")))
			continue;

		switch (output) {
			case OUTPUT_BLOCKDEV:
				printf ("%s\n", ENT_BLOCKDEVICE (ent));
				break;
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
