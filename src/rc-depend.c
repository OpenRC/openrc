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

#include "einfo.h"
#include "rc.h"
#include "rc-misc.h"
#include "strlist.h"

int main (int argc, char **argv)
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
				rc_update_deptree (true);
				update = true;
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
			types = rc_strlist_add (types, argv[i]);
		} else {
			if ((deptree = rc_load_deptree ()) == NULL)
				eerrorx ("failed to load deptree");

			di = rc_get_depinfo (deptree, argv[i]);
			if (! di)
				eerror ("no dependency info for service `%s'", argv[i]);
			else
				services = rc_strlist_add (services, argv[i]);
		}
	}

	if (! services) {
		rc_strlist_free (types);
		rc_free_deptree (deptree);
		if (update)
			return (EXIT_SUCCESS);
		eerrorx ("no services specified");
	}

	/* If we don't have any types, then supply some defaults */
	if (! types) {
		types = rc_strlist_add (NULL, "ineed");
		rc_strlist_add (types, "iuse");
	}

	depends = rc_get_depends (deptree, types, services, runlevel, options);

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
	rc_free_deptree (deptree);

	return (EXIT_SUCCESS);
}
