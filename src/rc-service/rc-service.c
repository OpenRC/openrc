/*
 * rc-service.c
 * Finds all OpenRC services
 */

/*
 * Copyright (c) 2008-2015 The OpenRC Authors.
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
#include "_usage.h"
#include "helpers.h"

const char *applet = NULL;
const char *extraopts = NULL;
#define opts(opt, opt_long) \
	opt(d, debug, no_argument, "set xtrace when running the command") \
	opt(D, nodeps, no_argument, "ignore dependencies") \
	opt(e, exists, required_argument, "tests if the service exists or not") \
	opt(c, ifcrashed, no_argument, "if the service is crashed run the command") \
	opt(i, ifexists, no_argument, "if the service exists run the command") \
	opt(I, ifinactive, no_argument, "if the service is inactive run the command") \
	opt(N, ifnotstarted, no_argument, "if the service is not started run the command") \
	opt(s, ifstarted, no_argument, "if the service is started run the command") \
	opt(S, ifstopped, no_argument, "if the service is stopped run the command") \
	opt(l, list, no_argument, "list all available services") \
	opt(r, resolve, required_argument, "resolve the service name to an init script") \
	opt(Z, dry-run, no_argument, "dry run (show what would happen)") \
	opt(U, user, no_argument, "Run in user mode")
cmdline_opts(opts)

const char *usagestring = ""
	"Usage: rc-service [options] [-i] <service> <cmd>...\n"
	"   or: rc-service [options] -e <service>\n"
	"   or: rc-service [options] -l\n"
	"   or: rc-service [options] -r <service>";

int main(int argc, char **argv)
{
	int opt;
	char *service;
	RC_STRINGLIST *list;
	RC_STRING *s;
	RC_SERVICE state;
	bool if_crashed = false;
	bool if_exists = false;
	bool if_inactive = false;
	bool if_notstarted = false;
	bool if_started = false;
	bool if_stopped = false;

	applet = basename_c(argv[0]);
	/* Ensure that we are only quiet when explicitly told to be */
	unsetenv("EINFO_QUIET");

	while ((opt = getopt_long(argc, argv, getoptstring,
		    longopts, (int *) 0)) != -1)
	{
		switch (opt) {
		case 'd':
			setenv("RC_DEBUG", "yes", 1);
			break;
		case 'D':
			setenv("RC_NODEPS", "yes", 1);
			break;
		case 'e':
			service = rc_service_resolve(optarg);
			opt = service ? EXIT_SUCCESS : EXIT_FAILURE;
			free(service);
			return opt;
			/* NOTREACHED */
		case 'c':
			if_crashed = true;
			break;
		case 'i':
			if_exists = true;
			break;
		case 'I':
			if_inactive = true;
			break;
		case 'N':
			if_notstarted = true;
			break;
		case 'l':
			list = rc_services_in_runlevel(NULL);
			if (TAILQ_FIRST(list) == NULL)
				return EXIT_FAILURE;
			rc_stringlist_sort(&list);
			TAILQ_FOREACH(s, list, entries)
			    printf("%s\n", s->value);
			rc_stringlist_free(list);
			return EXIT_SUCCESS;
			/* NOTREACHED */
		case 'r':
			service = rc_service_resolve(optarg);
			if (service == NULL)
				return EXIT_FAILURE;
			printf("%s\n", service);
			free(service);
			return EXIT_SUCCESS;
			/* NOTREACHED */
		case 's':
			if_started = true;
			break;
		case 'S':
			if_stopped = true;
			break;
		case 'Z':
			setenv("IN_DRYRUN", "yes", 1);
			break;

		case_RC_COMMON_GETOPT
		}
	}

	argc -= optind;
	argv += optind;
	if (*argv == NULL)
		eerrorx("%s: you need to specify a service", applet);
	if ((service = rc_service_resolve(*argv)) == NULL) {
		if (if_exists)
			return 0;
		eerrorx("%s: service `%s' does not exist", applet, *argv);
	}
	state = rc_service_state(*argv);
	if (if_crashed &&  !(rc_service_daemons_crashed(*argv) && errno != EACCES))
		return 0;
	if (if_inactive && !(state & RC_SERVICE_INACTIVE))
		return 0;
	if (if_notstarted && (state & RC_SERVICE_STARTED))
		return 0;
	if (if_started && !(state & RC_SERVICE_STARTED))
		return 0;
	if (if_stopped && !(state & RC_SERVICE_STOPPED))
		return 0;
	*argv = service;
	execv(*argv, argv);
	eerrorx("%s: %s", applet, strerror(errno));
	/* NOTREACHED */
}
