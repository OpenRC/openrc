/*
   rc-depend
   rc service dependency and ordering
   Copyright 2006-2007 Gentoo Foundation
   Released under the GPLv2
   */

#define APPLET "rc-depend"

#include <sys/types.h>
#include <sys/stat.h>

#include <getopt.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "builtins.h"
#include "einfo.h"
#include "rc.h"
#include "rc-misc.h"
#include "strlist.h"

rc_depinfo_t *_rc_deptree_load (void) {
	if (rc_deptree_update_needed ()) {
		int retval;

		ebegin ("Caching service dependencies");
		retval = rc_deptree_update ();
		eend (retval ? 0 : -1, "Failed to update the dependency tree");
	}

	return (rc_deptree_load ());
}

static char *applet = NULL;

#include "_usage.h"
#define getoptstring "t:suT" getoptstring_COMMON
static struct option longopts[] = {
	{ "type",     0, NULL, 't'},
	{ "notrace",  0, NULL, 'T'},
	{ "strict",   0, NULL, 's'},
	{ "update",   0, NULL, 'u'},
	longopts_COMMON
};
static const char * const longopts_help[] = {
	"Type(s) of dependency to list",
	"Don't trace service dependencies",
	"Only use what is in the runlevels",
	"Force an update of the dependency tree",
	longopts_help_COMMON
};
#include "_usage.c"

int rc_depend (int argc, char **argv)
{
	char **types = NULL;
	char **services = NULL;
	char **depends = NULL;
	char **list;
	rc_depinfo_t *deptree = NULL;
	char *service;
	int options = RC_DEP_TRACE;
	bool first = true;
	int i;
	bool update = false;
	char *runlevel = getenv ("RC_SOFTLEVEL");
	int opt;
	char *token;

	applet = argv[0];

	while ((opt = getopt_long (argc, argv, getoptstring,
							   longopts, (int *) 0)) != -1)
	{
		switch (opt) {
			case 's':
				options |= RC_DEP_STRICT;
				break;
			case 't':
				while ((token = strsep (&optarg, ",")))
					rc_strlist_addu (&types, token);
				break;
			case 'u':
				update = true;
				break;
			case 'T':
				options &= RC_DEP_TRACE;
				break;

			case_RC_COMMON_GETOPT
		}
	}

	if (update) {
		bool u = false;
		ebegin ("Caching service dependencies");
		u = rc_deptree_update ();
		eend (u ? 0 : -1, "%s: %s", applet, strerror (errno));
		if (! u)
			eerrorx ("Failed to update the dependency tree");
	}

	if (! runlevel)
		runlevel = rc_runlevel_get ();

	if (! (deptree = _rc_deptree_load ()))
		eerrorx ("failed to load deptree");

	while (optind < argc) {
		list = NULL;
		rc_strlist_add (&list, argv[optind]);
		errno = 0;
		depends = rc_deptree_depends (deptree, NULL, list, runlevel, 0);
		if (! depends && errno == ENOENT)
			eerror ("no dependency info for service `%s'", argv[optind]);
		else
			rc_strlist_add (&services, argv[optind]);

		rc_strlist_free (depends);
		rc_strlist_free (list);
		optind++;
	}

	if (! services) {
		rc_strlist_free (types);
		rc_deptree_free (deptree);
		if (update)
			return (EXIT_SUCCESS);
		eerrorx ("no services specified");
	}

	/* If we don't have any types, then supply some defaults */
	if (! types) {
		rc_strlist_add (&types, "ineed");
		rc_strlist_add (&types, "iuse");
	}

	depends = rc_deptree_depends (deptree, types, services, runlevel, options);

	if (depends) {
		STRLIST_FOREACH (depends, service, i) {
			if (first)
				first = false;
			else
				printf (" ");

			if (service)
				printf ("%s", service);

		}
		printf ("\n");
	}

	rc_strlist_free (types);
	rc_strlist_free (services);
	rc_strlist_free (depends);
	rc_deptree_free (deptree);
	return (EXIT_SUCCESS);
}
