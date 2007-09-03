/*
   mountinfo.c
   Obtains information about mounted filesystems.

   Copyright 2007 Gentoo Foundation
   */

#define APPLET "mountinfo"

#include <sys/types.h>

#if defined(__DragonFly__) || defined(__FreeBSD__) || \
	defined(__NetBSD__) || defined(__OpenBSD__)
#define BSD
#include <sys/param.h>
#include <sys/ucred.h>
#include <sys/mount.h>
#endif

#include <errno.h>
#include <getopt.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <regex.h>

#include "builtins.h"
#include "einfo.h"
#include "rc.h"
#include "rc-misc.h"
#include "strlist.h"


#ifdef BSD 
static char **find_mounts (regex_t *node_regex, regex_t *skip_node_regex,
						   regex_t *fstype_regex, regex_t *skip_fstype_regex,
						   char **mounts, bool node, bool fstype)
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
		if (skip_node_regex &&
			regexec (skip_node_regex, mnts[i].f_mntfromname, 0, NULL, 0) == 0)
			continue;
		if (fstype_regex &&
			regexec (fstype_regex, mnts[i].f_fstypename, 0, NULL, 0) != 0)
			continue;
		if (skip_fstype_regex &&
			regexec (skip_fstype_regex, mnts[i].f_fstypename, 0, NULL, 0) == 0)
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

		list = rc_strlist_addsortc (list, node ?
									mnts[i].f_mntfromname :
									fstype ? mnts[i].f_fstypename :
									mnts[i].f_mntonname);
	}

	return (list);
}

#elif defined (__linux__)
static char **find_mounts (regex_t *node_regex, regex_t *skip_node_regex,
						   regex_t *fstype_regex, regex_t *skip_fstype_regex,
						   char **mounts, bool node, bool fstype)
{
	FILE *fp;
	char buffer[PATH_MAX * 3];
	char *p;
	char *from;
	char *to;
	char *fst;
	char **list = NULL;

	if ((fp = fopen ("/proc/mounts", "r")) == NULL)
		eerrorx ("getmntinfo: %s", strerror (errno));

	while (fgets (buffer, sizeof (buffer), fp)) {
		p = buffer;
		from = strsep (&p, " ");
		if (node_regex &&
			regexec (node_regex, from, 0, NULL, 0) != 0)
			continue;
		if (skip_node_regex &&
			regexec (skip_node_regex, from, 0, NULL, 0) == 0)
			continue;

		to = strsep (&p, " ");
		fst = strsep (&p, " ");
		/* Skip the really silly rootfs */
		if (strcmp (fst, "rootfs") == 0)
			continue;
		if (fstype_regex &&
			regexec (fstype_regex, fst, 0, NULL, 0) != 0)
			continue;
		if (skip_fstype_regex &&
			regexec (skip_fstype_regex, fst, 0, NULL, 0) == 0)
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

		list = rc_strlist_addsortc (list, node ? from : fstype ? fst : to);
	}
	fclose (fp);

	return (list);
}

#else
#  error "Operating system not supported!"
#endif

static regex_t *get_regex (char *string)
{
	regex_t *reg = rc_xmalloc (sizeof (regex_t));
	int result;
	char buffer[256];

	if ((result = regcomp (reg, string, REG_EXTENDED | REG_NOSUB)) != 0)
	{
		regerror (result, reg, buffer, sizeof (buffer));
		eerrorx ("%s: invalid regex `%s'", APPLET, buffer);
	}

	return (reg);
}

#include "_usage.h"
#define getoptstring "f:F:n:N:op:P:qs" getoptstring_COMMON
static struct option longopts[] = {
	{ "fstype-regex",        1, NULL, 'f'},
	{ "skip-fstype-regex",   1, NULL, 'F'},
	{ "node-regex",          1, NULL, 'n'},
	{ "skip-node-regex",     1, NULL, 'N'},
	{ "point-regex",         1, NULL, 'p'},
	{ "skip-point-regex",    1, NULL, 'P'},
	{ "node",                0, NULL, 'o'},
	{ "fstype",              0, NULL, 's'},
	{ "quiet",               0, NULL, 'q'},
	longopts_COMMON
	{ NULL,             0, NULL, 0}
};
#include "_usage.c"

int mountinfo (int argc, char **argv)
{
	int i;
	regex_t *fstype_regex = NULL;
	regex_t *node_regex = NULL;
	regex_t *point_regex = NULL;
	regex_t *skip_fstype_regex = NULL;
	regex_t *skip_node_regex = NULL;
	regex_t *skip_point_regex = NULL;
	char **nodes = NULL;
	char *n;
	bool node = false;
	bool fstype = false;
	char **mounts = NULL;
	int opt;
	bool quiet = false;
	int result;

#define DO_REG(_var) \
	if (_var) free (_var); \
	_var = get_regex (optarg);

	while ((opt = getopt_long (argc, argv, getoptstring,
							   longopts, (int *) 0)) != -1)
	{
		switch (opt) {
			case 'f':
				DO_REG (fstype_regex);
				break;
			case 'F':
				DO_REG (skip_fstype_regex);
				break;
			case 'n':
				DO_REG (node_regex);
				break;
			case 'N':
				DO_REG (skip_node_regex);
				break;
			case 'o':
				node = true;
				fstype = false;
				break;
			case 'p':
				DO_REG (point_regex);
				break;
			case 'P':
				DO_REG (skip_point_regex);
				break;
			case 'q':
				quiet = true;
				break;
			case 's':
				node = false;
				fstype = true;
				break;

				case_RC_COMMON_GETOPT
		}
	}

	while (optind < argc) {
		if (argv[optind][0] != '/')
			eerrorx ("%s: `%s' is not a mount point", argv[0], argv[optind]);
		mounts = rc_strlist_add (mounts, argv[optind++]);
	}

	nodes = find_mounts (node_regex, skip_node_regex,
						 fstype_regex, skip_fstype_regex,
						 mounts, node, fstype);

	if (node_regex)
		regfree (node_regex);
	if (skip_node_regex)
		regfree (skip_node_regex);
	if (fstype_regex)
		regfree (fstype_regex);
	if (skip_fstype_regex)
		regfree (skip_fstype_regex);

	rc_strlist_reverse (nodes);

	result = EXIT_FAILURE;
	STRLIST_FOREACH (nodes, n, i) {
		if (point_regex && regexec (point_regex, n, 0, NULL, 0) != 0)
			continue;
		if (skip_point_regex && regexec (skip_point_regex, n, 0, NULL, 0) == 0)
			continue;
		if (! quiet)
			printf ("%s\n", n);
		result = EXIT_SUCCESS;
	}
	rc_strlist_free (nodes);

	if (point_regex)
		regfree (point_regex);
	if (skip_point_regex)
		regfree (skip_point_regex);

	exit (result);
}
