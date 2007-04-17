/*
   mountinfo.c
   Obtains information about mounted filesystems.

   Copyright 2007 Gentoo Foundation
   */

#define APPLET "mountinfo"

#include <sys/types.h>
#if defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
#include <sys/param.h>
#include <sys/ucred.h>
#include <sys/mount.h>
#elif defined(__linux__)
#include <limits.h>
#endif

#include <errno.h>
#include <getopt.h>
#include <limits.h>
#include <regex.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "einfo.h"
#include "rc.h"
#include "rc-misc.h"
#include "strlist.h"


#if defined(__FreeBSD__) || defined(__NetBSD__) || defined (__OpenBSD__)
static char **find_mounts (regex_t *node_regex, regex_t *fstype_regex,
						   char **mounts, bool list_nodes, bool list_fstype)
{
	struct statfs *mnts;
	int nmnts;
	int i;
	char **list = NULL;

	if ((nmnts = getmntinfo (&mnts, MNT_NOWAIT)) == 0)
		eerrorx ("getmntinfo: %s", strerror (errno));

	for (i = 0; i < nmnts; i++) {
		if (node_regex &&
			regexec (node_regex, mnts[i].f_mntfromname, 0, NULL, 0) != 0)
			continue;
		if (fstype_regex &&
			regexec (fstype_regex, mnts[i].f_fstypename, 0, NULL, 0) != 0)
			continue;

		if (mounts) {
			bool found = false;
			int j;
			char *mnt;
			STRLIST_FOREACH (mounts, mnt, j)
				if (strcmp (mnt, mnts[i].f_mntonname) == 0) {
					found = true;
					break;
				}
			if (! found)
				continue;
		}

		list = rc_strlist_addsortc (list, list_nodes ?
									mnts[i].f_mntfromname :
									list_fstype ? mnts[i].f_fstypename :
									mnts[i].f_mntonname);
	}

	return (list);
}

#elif defined (__linux__)
static char **find_mounts (regex_t *node_regex, regex_t *fstype_regex,
						   char **mounts, bool list_nodes, bool list_fstype)
{
	FILE *fp;
	char buffer[PATH_MAX * 3];
	char *p;
	char *from;
	char *to;
	char *fstype;
	char **list = NULL;

	if ((fp = fopen ("/proc/mounts", "r")) == NULL)
		eerrorx ("getmntinfo: %s", strerror (errno));

	while (fgets (buffer, sizeof (buffer), fp)) {
		p = buffer;
		from = strsep (&p, " ");
		if (node_regex &&
			regexec (node_regex, from, 0, NULL, 0) != 0)
			continue;

		to = strsep (&p, " ");
		fstype = strsep (&p, " ");
		/* Skip the really silly rootfs */
		if (strcmp (fstype, "rootfs") == 0)
			continue;
		if (fstype_regex &&
			regexec (fstype_regex, fstype, 0, NULL, 0) != 0)
			continue;

		if (mounts)	{
			bool found = false;
			int j;
			char *mnt;
			STRLIST_FOREACH (mounts, mnt, j)
				if (strcmp (mnt, to) == 0) {
					found = true;
					break;
				}
			if (! found)
				continue;
		}

		list = rc_strlist_addsortc (list,
									list_nodes ? 
									list_fstype ? fstype :
									from : to);
	}
	fclose (fp);

	return (list);
}

#else
#  error "Operating system not supported!"
#endif

#define getoptstring "F:N:S:fnrhV"
static struct option longopts[] = {
	{ "fstype-regex",   1, NULL, 'F'},
	{ "node-regex",     1, NULL, 'N'},
	{ "skip-regex",     1, NULL, 'S'},
	{ "fstype",         0, NULL, 'f'},
	{ "node",           0, NULL, 'n'},
	{ "reverse",        0, NULL, 'r'},
	{ "help",           0, NULL, 'h'},
	{ NULL,             0, NULL, 0}
};
#include "_usage.c"

int main (int argc, char **argv)
{
	int i;
	regex_t *fstype_regex = NULL;
	regex_t *node_regex = NULL;
	regex_t *skip_regex = NULL;
	char **nodes = NULL;
	char *node;
	int result;
	char buffer[256];
	bool list_nodes = false;
	bool list_fstype = false;
	bool reverse = false;
	char **mounts = NULL;
	char c;

	while ((c = getopt_long (argc, argv, getoptstring,
							 longopts, (int *) 0)) != -1)
		switch (c) {
			case 'F':
				if (fstype_regex)
					free (fstype_regex);
				fstype_regex = rc_xmalloc (sizeof (regex_t));
				if ((result = regcomp (fstype_regex, optarg,
									   REG_EXTENDED | REG_NOSUB)) != 0)
				{
					regerror (result, fstype_regex, buffer, sizeof (buffer));
					eerrorx ("%s: invalid regex `%s'", argv[0], buffer);
				}
				break;

			case 'N':
				if (node_regex)
					free (node_regex);
				node_regex = rc_xmalloc (sizeof (regex_t));
				if ((result = regcomp (node_regex, optarg,
									   REG_EXTENDED | REG_NOSUB)) != 0)
				{
					regerror (result, node_regex, buffer, sizeof (buffer));
					eerrorx ("%s: invalid regex `%s'", argv[0], buffer);
				}
				break;

			case 'S':
				if (skip_regex)
					free (skip_regex);
				skip_regex = rc_xmalloc (sizeof (regex_t));
				if ((result = regcomp (skip_regex, optarg,
									   REG_EXTENDED | REG_NOSUB)) != 0)
				{
					regerror (result, skip_regex, buffer, sizeof (buffer));
					eerrorx ("%s: invalid regex `%s'", argv[0], buffer);
				}
				break;

			case 'f':
				list_fstype = true;
				list_nodes = false;
				break;

			case 'n':
				list_nodes = true;
				list_fstype = false;
				break;

			case 'r':
				reverse = true;
				break;

			case_RC_COMMON_GETOPT
		}

	while (optind < argc) {
		if (argv[optind][0] != '/')
			eerrorx ("%s: `%s' is not a mount point", argv[0], argv[optind]);
		mounts = rc_strlist_add (mounts, argv[optind++]);
	}
	nodes = find_mounts (node_regex, fstype_regex, mounts,
						 list_nodes, list_fstype);

	if (node_regex)
		regfree (node_regex);
	if (fstype_regex)
		regfree (fstype_regex);

	if (reverse)
		rc_strlist_reverse (nodes);

	result = EXIT_FAILURE;
	STRLIST_FOREACH (nodes, node, i) {
		if (skip_regex && regexec (skip_regex, node, 0, NULL, 0) == 0)
			continue;
		printf ("%s\n", node);
		result = EXIT_SUCCESS;
	}
	rc_strlist_free (nodes);

	if (skip_regex)
		free (skip_regex);

	exit (result);
}
