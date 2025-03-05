/*
 * rc-update
 * Manage init scripts and runlevels
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
const char *usagestring = ""							\
	"Usage: rc-update [options] add <service> [<runlevel>...]\n"	\
	"   or: rc-update [options] del <service> [<runlevel>...]\n"	\
	"   or: rc-update [options] env <variables>...\n"	\
	"   or: rc-update [options] [show [<runlevel>...]]";
const char getoptstring[] = "asu" getoptstring_COMMON;
const struct option longopts[] = {
	{ "all",             0, NULL, 'a' },
	{ "stack",           0, NULL, 's' },
	{ "update",          0, NULL, 'u' },
	longopts_COMMON
};
const char * const longopts_help[] = {
	"Process all runlevels",
	"Stack a runlevel instead of a service",
	"Force an update of the dependency tree",
	longopts_help_COMMON
};

extern const char **environ;

/* Return the number of changes made:
 *  -1 = no changes (error)
 *   0 = no changes (nothing to do)
 *  1+ = number of runlevels updated
 */
static int
add(const char *runlevel, const char *service)
{
	if (!rc_service_exists(service)) {
		if (errno == ENOEXEC)
			eerror("%s: service '%s' is not executable", applet, service);
		else
			eerror("%s: service '%s' does not exist", applet, service);
		return -1;
	}

	if (rc_service_in_runlevel(service, runlevel)) {
		einfo("%s: %s already installed in runlevel '%s'; skipping", applet, service, runlevel);
		return 0;
	}

	if (rc_service_add(runlevel, service)) {
		einfo("%s: service %s added to runlevel %s", applet, service, runlevel);
		return 1;
	}

	eerror("%s: failed to add service '%s' to runlevel '%s': %s", applet, service, runlevel, strerror(errno));
	return -1;
}

static int
delete(const char *runlevel, const char *service)
{
	if (rc_service_delete(runlevel, service)) {
		einfo("%s: service %s deleted from runlevel %s", applet, service, runlevel);
		return 1;
	}

	if (errno == ENOENT)
		eerror("%s: service '%s' is not in the runlevel '%s'", applet, service, runlevel);
	else
		eerror("%s: failed to delete service '%s' from runlevel '%s': %s", applet, service, runlevel, strerror(errno));

	return -1;
}

static int
addstack(const char *runlevel, const char *stack)
{
	if (!rc_runlevel_exists(runlevel)) {
		eerror("%s: runlevel '%s' does not exist", applet, runlevel);
		return -1;
	}

	if (!rc_runlevel_exists(stack)) {
		eerror("%s: runlevel '%s' does not exist", applet, stack);
		return -1;
	}

	if (strcmp(runlevel, stack) == 0) {
		eerror("%s: cannot stack '%s' onto itself", applet, stack);
		return -1;
	}

	if (strcmp(runlevel, RC_LEVEL_SYSINIT) == 0 || strcmp(stack, RC_LEVEL_SYSINIT) == 0 ||
			strcmp(runlevel, RC_LEVEL_BOOT) == 0 || strcmp(stack, RC_LEVEL_BOOT) == 0 ||
			strcmp(runlevel, RC_LEVEL_SINGLE) == 0 || strcmp(stack, RC_LEVEL_SINGLE) == 0 ||
			strcmp(runlevel, RC_LEVEL_SHUTDOWN) == 0 || strcmp(stack, RC_LEVEL_SHUTDOWN) == 0) {
		eerror("%s: cannot stack the %s runlevel", applet, RC_LEVEL_SYSINIT);
		return -1;
	}

	if (!rc_runlevel_stack(runlevel, stack)) {
		eerror("%s: failed to stack '%s' to '%s': %s", applet, stack, runlevel, strerror(errno));
		return -1;
	}

	einfo("%s: runlevel %s added to runlevel %s", applet, stack, runlevel);
	return 1;
}

static int
delstack(const char *runlevel, const char *stack)
{
	if (rc_runlevel_unstack(runlevel, stack)) {
		einfo("%s: runlevel %s deleted from runlevel %s", applet, stack, runlevel);
		return 1;
	}

	if (errno == ENOENT)
		eerror("%s: runlevel '%s' is not in the runlevel '%s'", applet, stack, runlevel);
	else
		eerror("%s: failed to delete runlevel '%s' from runlevel '%s': %s", applet, stack, runlevel, strerror(errno));

	return -1;
}

static void
show(RC_STRINGLIST *runlevels, bool verbose)
{
	RC_STRINGLIST *services = rc_services_in_runlevel(NULL);
	RC_STRING *service;
	RC_STRING *runlevel;
	RC_STRINGLIST *in;
	bool inone;
	char *buffer = NULL;
	size_t l;

	rc_stringlist_sort(&services);
	TAILQ_FOREACH(service, services, entries) {
		in = rc_stringlist_new();
		inone = false;

		TAILQ_FOREACH(runlevel, runlevels, entries) {
			if (rc_service_in_runlevel(service->value, runlevel->value)) {
				rc_stringlist_add(in, runlevel->value);
				inone = true;
				continue;
			}

			l = strlen(runlevel->value);
			buffer = xmalloc(l+1);
			memset(buffer, ' ', l);
			buffer[l] = 0;
			rc_stringlist_add(in, buffer);
			free(buffer);
		}

		if (inone || verbose) {
			printf(" %20s |", service->value);
			TAILQ_FOREACH(runlevel, in, entries)
				printf (" %s", runlevel->value);
			printf("\n");
		}
		rc_stringlist_free(in);
	}

	rc_stringlist_free(services);
}

static void
env(char *variable)
{
	char *value = variable;
	variable = strsep(&value, "=");

	if (!value && !(value = getenv(variable))) {
		ewarn("%s: environment variable %s not set, skipping.", applet, variable);
		return;
	}

	rc_export_variable(variable, value);
}

enum update_action {
	DO_NONE,
	DO_ADD,
	DO_DELETE,
	DO_SHOW,
	DO_ENV
};

int main(int argc, char **argv)
{
	RC_DEPTREE *deptree;
	RC_STRINGLIST *runlevels;
	RC_STRING *runlevel;
	char *service = NULL;
	char *p;
	enum update_action action = DO_NONE;
	bool verbose = false, stack = false, all_runlevels = false;
	int opt;
	int retval = EXIT_FAILURE;
	int num_updated = 0;
	int (*actfunc)(const char *, const char *);
	int ret;

	applet = basename_c(argv[0]);
	while ((opt = getopt_long(argc, argv, getoptstring, longopts, NULL)) != -1) {
		switch (opt) {
		case 'a':
		    all_runlevels = true;
		break;
		case 's':
		    stack = true;
		break;
		case 'u':
			deptree = _rc_deptree_load(-1, &ret);
			if (deptree)
				rc_deptree_free(deptree);
			return ret;
		case_RC_COMMON_GETOPT
		}
	}

	verbose = rc_yesno(getenv("EINFO_VERBOSE"));
	if (optind < argc) {
		if (strcmp(argv[optind], "add") == 0)
			action = DO_ADD;
		else if (strcmp(argv[optind], "delete") == 0 ||
		    strcmp(argv[optind], "del") == 0)
			action = DO_DELETE;
		else if (strcmp(argv[optind], "show") == 0)
			action = DO_SHOW;
		else if (strcmp(argv[optind], "env") == 0)
			action = DO_ENV;
		else
			usage(EXIT_FAILURE);
		optind++;
	}
	if (action == DO_NONE)
		action = DO_SHOW;

	if (optind >= argc && action != DO_SHOW)
		usage(EXIT_FAILURE);

	if (action != DO_ENV) {
		service = argv[optind];
		optind++;

		runlevels = rc_stringlist_new();
		while (optind < argc) {
			if (rc_runlevel_exists(argv[optind])) {
				rc_stringlist_add(runlevels, argv[optind++]);
			} else {
				rc_stringlist_free(runlevels);
				eerrorx("%s: '%s' is not a valid runlevel", applet, argv[optind]);
			}
		}
	}

	retval = EXIT_SUCCESS;

	switch (action) {
	case DO_NONE:
		break;
	case DO_SHOW:
		if (service)
			rc_stringlist_add(runlevels, service);

		if (!TAILQ_FIRST(runlevels)) {
			rc_stringlist_free(runlevels);
			runlevels = rc_runlevel_list();
		}

		rc_stringlist_sort(&runlevels);
		show(runlevels, verbose);
		goto exit;
	case DO_ENV:
		while (optind < argc)
			env(argv[optind++]);
		return 0;
	case DO_ADD:
		if (all_runlevels) {
			rc_stringlist_free(runlevels);
			eerrorx("%s: the -a option is invalid with add", applet);
		}
		actfunc = stack ? addstack : add;
		break;
	case DO_DELETE:
		actfunc = stack ? delstack : delete;
		break;
	}

	if (!TAILQ_FIRST(runlevels)) {
		if (all_runlevels) {
			free(runlevels);
			runlevels = rc_runlevel_list();
		} else {
			p = rc_runlevel_get();
			rc_stringlist_add(runlevels, p);
			free(p);
		}
	}

	if (!TAILQ_FIRST(runlevels)) {
		free(runlevels);
		eerrorx("%s: no runlevels found", applet);
	}

	TAILQ_FOREACH(runlevel, runlevels, entries) {
		ret = actfunc(runlevel->value, service);
		if (ret < 0)
			retval = EXIT_FAILURE;
		num_updated += ret;
	}

	if (retval == EXIT_SUCCESS && num_updated == 0 && action == DO_DELETE)
		ewarnx("%s: service '%s' not found in any of the specified runlevels", applet, service);

exit:
	rc_stringlist_free(runlevels);
	return retval;
}
