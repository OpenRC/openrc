/*
   rc-depend
   rc service dependency and ordering
   Copyright 2006-2007 Gentoo Foundation
   Released under the GPLv2
   */

#include <sys/types.h>
#include <sys/stat.h>

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
		eend (retval, "Failed to update the dependency tree");
	}

	return (rc_deptree_load ());
}

int rc_depend (int argc, char **argv)
{
	char **types = NULL;
	char **services = NULL;
	char **depends = NULL;
	rc_depinfo_t *deptree = NULL;
	rc_depinfo_t *di;
	char *service;
	int options = RC_DEP_TRACE;
	bool first = true;
	int i;
	bool update = false;
	char *runlevel = getenv ("RC_SOFTLEVEL");

	if (! runlevel)
		runlevel = rc_get_runlevel ();

	for (i = 1; i < argc; i++) {
		if (strcmp (argv[i], "--update") == 0) {
			if (! update) {
				ebegin ("Caching service dependencies");
				update = (rc_deptree_update () == 0);
				eend (update ? 0 : -1, "Failed to update the dependency tree");
			}
			continue;
		}

		if (strcmp (argv[i], "--strict") == 0) {
			options |= RC_DEP_STRICT;
			continue;
		}

		if (strcmp (argv[i], "--notrace") == 0) {
			options &= RC_DEP_TRACE;
			continue;
		}

		if (argv[i][0] == '-') {
			argv[i]++;
			rc_strlist_add (&types, argv[i]);
		} else {
			if ((deptree = _rc_deptree_load ()) == NULL)
				eerrorx ("failed to load deptree");

			di = rc_deptree_depinfo (deptree, argv[i]);
			if (! di)
				eerror ("no dependency info for service `%s'", argv[i]);
			else
				rc_strlist_add (&services, argv[i]);
		}
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
