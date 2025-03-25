/*
 * rc-depend
 * rc service dependency and ordering
 */

/*
 * Copyright (c) 2007-2015 The OpenRC Authors.
 * See the Authors file at the top-level directory of this distribution and
 * https://github.com/OpenRC/openrc/blob/HEAD/AUTHORS
 *
 * This file is part of OpenRC. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution and at https://github.com/OpenRC/openrc/blob/HEAD/LICENSE
 * This file may not be copied, modified, propagated, or distributed
 *    except according to the terms contained in the LICENSE file.
 */

#include <errno.h>
#include <getopt.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "einfo.h"
#include "queue.h"
#include "rc.h"
#include "misc.h"
#include "_usage.h"
#include "helpers.h"

const char *applet = NULL;
const char *extraopts = NULL;
const char getoptstring[] = "aot:suTF:" getoptstring_COMMON;
const struct option longopts[] = {
	{ "starting", 0, NULL, 'a'},
	{ "stopping", 0, NULL, 'o'},
	{ "type",     1, NULL, 't'},
	{ "notrace",  0, NULL, 'T'},
	{ "strict",   0, NULL, 's'},
	{ "update",   0, NULL, 'u'},
	{ "deptree-file", 1, NULL, 'F'},
	longopts_COMMON
};
const char * const longopts_help[] = {
	"Order services as if runlevel is starting",
	"Order services as if runlevel is stopping",
	"Type(s) of dependency to list",
	"Don't trace service dependencies",
	"Only use what is in the runlevels",
	"Force an update of the dependency tree",
	"File to load cached deptree from",
	longopts_help_COMMON
};
const char *usagestring = NULL;

int main(int argc, char **argv)
{
	RC_STRINGLIST *list;
	enum rc_deptype types;
	RC_STRINGLIST *services;
	RC_STRINGLIST *depends;
	RC_STRING *s;
	bool default_types = true;
	RC_DEPTREE *deptree = NULL;
	int options = RC_DEP_TRACE, update = 0;
	bool first = true;
	char *runlevel = xstrdup(getenv("RC_RUNLEVEL"));
	int opt;
	char *token;
	char *deptree_file = NULL;

	applet = basename_c(argv[0]);
	while ((opt = getopt_long(argc, argv, getoptstring,
		    longopts, (int *) 0)) != -1)
	{
		switch (opt) {
		case 'a':
			options |= RC_DEP_START;
			break;
		case 'o':
			options |= RC_DEP_STOP;
			break;
		case 's':
			options |= RC_DEP_STRICT;
			break;
		case 't':
			default_types = false;
			while ((token = strsep(&optarg, ","))) {
				enum rc_deptype parsed = rc_deptype_parse(token);
				if (parsed == RC_DEPTYPE_INVALID)
					eerrorx("Invalid deptype '%s'", token);
				types |= (1 << parsed);
			}
			break;
		case 'u':
			update = 1;
			break;
		case 'T':
			options &= RC_DEP_TRACE;
			break;
		case 'F':
			deptree_file = xstrdup(optarg);
			break;

		case_RC_COMMON_GETOPT
		}
	}

	if (deptree_file) {
		if (!(deptree = rc_deptree_load_file(deptree_file)))
			eerrorx("failed to load deptree");
	} else {
		if (!(deptree = _rc_deptree_load(update, NULL)))
			eerrorx("failed to load deptree");
	}

	if (!runlevel)
		runlevel = rc_runlevel_get();

	services = rc_stringlist_new();
	while (optind < argc) {
		//list = rc_stringlist_new();
		//rc_stringlist_add(list, argv[optind]);
		//errno = 0;
		/*
		depends = rc_deptree_depends(deptree, -1, list, runlevel, 0);
		if (!depends && errno == ENOENT)
			eerror("no dependency info for service `%s'",
			    argv[optind]);
		else
		*/
		rc_stringlist_add(services, argv[optind]);

		//rc_stringlist_free(depends);
		//rc_stringlist_free(list);
		optind++;
	}
	if (!TAILQ_FIRST(services)) {
		rc_stringlist_free(services);
		rc_deptree_free(deptree);
		free(runlevel);
		if (update)
			return EXIT_SUCCESS;
		eerrorx("no services specified");
	}

	/* If we don't have any types, then supply some defaults */
	if (default_types)
		types = RC_DEP(INEED) | RC_DEP(IUSE);

	depends = rc_deptree_depends(deptree, types, services, runlevel, options);

	if (TAILQ_FIRST(depends)) {
		TAILQ_FOREACH(s, depends, entries) {
			if (first)
				first = false;
			else
				printf (" ");
			printf ("%s", s->value);

		}
		printf ("\n");
	}

	rc_stringlist_free(services);
	rc_stringlist_free(depends);
	rc_deptree_free(deptree);
	free(runlevel);
	return EXIT_SUCCESS;
}
