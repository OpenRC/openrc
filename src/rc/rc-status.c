/*
   rc-status
   Display the status of the services in runlevels
   */

/*
 * Copyright 2007-2008 Roy Marples
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

#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "builtins.h"
#include "einfo.h"
#include "rc.h"
#include "rc-misc.h"

extern const char *applet;
static bool test_crashed = false;
static const char *const types_nua[] = { "ineed", "iuse", "iafter", NULL };

bool _rc_can_find_pids(void)
{
	RC_PIDLIST *pids;
	RC_PID *pid;
	RC_PID *pid2;
	bool retval = false;

	if (geteuid() == 0)
		return true;

	/* If we cannot see process 1, then we don't test to see if
	 * services crashed or not */
	pids = rc_find_pids(NULL, NULL, 0, 1);
	if (pids) {
		pid = LIST_FIRST(pids);
		if (pid) {
			retval = true;
			while (pid) {
				pid2 = LIST_NEXT(pid, entries);
				free(pid);
				pid = pid2;
			}
		}
		free(pids);
	}

	return retval;
}

static void print_level(char *level)
{
	printf ("Runlevel: ");
	if (isatty(fileno(stdout)))
		printf("%s%s%s\n",
		       ecolor(ECOLOR_HILITE),
		       level,
		       ecolor(ECOLOR_NORMAL));
	else
		printf("%s\n", level);
}

static void print_service(const char *service)
{
	char status[10];
	int cols =  printf(" %s", service);
	const char *c = ecolor(ECOLOR_GOOD);
	RC_SERVICE state = rc_service_state(service);
	ECOLOR color = ECOLOR_BAD;

	if (state & RC_SERVICE_STOPPING)
		snprintf(status, sizeof(status), "stopping ");
	else if (state & RC_SERVICE_STARTING) {
		snprintf(status, sizeof(status), "starting ");
		color = ECOLOR_WARN;
	} else if (state & RC_SERVICE_INACTIVE) {
		snprintf(status, sizeof(status), "inactive ");
		color = ECOLOR_WARN;
	} else if (state & RC_SERVICE_STARTED) {
		if (test_crashed && rc_service_daemons_crashed(service))
			snprintf(status, sizeof(status), " crashed ");
		else {
			snprintf(status, sizeof(status), " started ");
			color = ECOLOR_GOOD;
		}
	} else if (state & RC_SERVICE_SCHEDULED) {
		snprintf(status, sizeof(status), "scheduled");
		color = ECOLOR_WARN;
	} else
		snprintf(status, sizeof(status), " stopped ");

	errno = 0;
	if (c && *c && isatty(fileno(stdout)))
		printf("\n");
	ebracket(cols, color, status);
}

#include "_usage.h"
#define extraopts "[runlevel1] [runlevel2] ..."
#define getoptstring "alrsu" getoptstring_COMMON
static const struct option longopts[] = {
	{"all",         0, NULL, 'a'},
	{"list",        0, NULL, 'l'},
	{"runlevel",    0, NULL, 'r'},
	{"servicelist", 0, NULL, 's'},
	{"unused",      0, NULL, 'u'},
	longopts_COMMON
};
static const char * const longopts_help[] = {
	"Show services from all run levels",
	"Show list of run levels",
	"Show the name of the current runlevel",
	"Show service list",
	"Show services not assigned to any runlevel",
	longopts_help_COMMON
};
#include "_usage.c"

int rc_status(int argc, char **argv)
{
	RC_DEPTREE *deptree = NULL;
	RC_STRINGLIST *levels = NULL;
	RC_STRINGLIST *services;
	RC_STRINGLIST *types = NULL;
	RC_STRINGLIST *ordered;
	RC_STRING *s;
	RC_STRING *l;
	char *p;
	int opt;
	int depopts = RC_DEP_STRICT | RC_DEP_START | RC_DEP_TRACE;

	test_crashed = _rc_can_find_pids();

	while ((opt = getopt_long(argc, argv, getoptstring, longopts,
				  (int *) 0)) != -1)
		switch (opt) {
		case 'a':
			levels = rc_runlevel_list();
			break;
		case 'l':
			levels = rc_runlevel_list();
			TAILQ_FOREACH (l, levels, entries)
				printf("%s\n", l->value);
			rc_stringlist_free(levels);
			exit(EXIT_SUCCESS);
			/* NOTREACHED */
		case 'r':
			p = rc_runlevel_get ();
			printf("%s\n", p);
			free(p);
			exit(EXIT_SUCCESS);
			/* NOTREACHED */
		case 's':
			services = rc_services_in_runlevel(NULL);
			TAILQ_FOREACH(s, services, entries)
				print_service(s->value);
			rc_stringlist_free(services);
			exit (EXIT_SUCCESS);
			/* NOTREACHED */
		case 'u':
			services = rc_services_in_runlevel(NULL);
			levels = rc_runlevel_list();
			TAILQ_FOREACH(s, services, entries) {
				TAILQ_FOREACH(l, levels, entries)
					if (rc_service_in_runlevel(s->value, l->value))
						break;
				if (! l)
						print_service(s->value);
			}
			rc_stringlist_free(levels);
			rc_stringlist_free(services);
			exit (EXIT_SUCCESS);
			/* NOTREACHED */

		case_RC_COMMON_GETOPT
		}

	if (! levels)
		levels = rc_stringlist_new();
	while (optind < argc)
		rc_stringlist_add(levels, argv[optind++]);
	if (! TAILQ_FIRST(levels)) {
		p = rc_runlevel_get();
		rc_stringlist_add(levels, p);
		free(p);
	}

	/* Output the services in the order in which they would start */
	deptree = _rc_deptree_load(NULL);

	TAILQ_FOREACH(l, levels, entries) {
		print_level(l->value);
		services = rc_services_in_runlevel(l->value);
		if (! services)
			continue;
		if (deptree) {
			if (! types) {
				types = rc_stringlist_new();
				rc_stringlist_add(types, "ineed");
				rc_stringlist_add(types, "iuse");
				rc_stringlist_add(types, "iafter");
			}
			ordered = rc_deptree_depends(deptree, types, services,
						     l->value, depopts);
			rc_stringlist_free(services);
			services = ordered;
			ordered = NULL;
		}
		TAILQ_FOREACH(s, services, entries)
			if (rc_service_in_runlevel(s->value, l->value))
				print_service(s->value);
		rc_stringlist_free(services);
	}

	rc_stringlist_free(types);
	rc_stringlist_free(levels);
	rc_deptree_free(deptree);

	exit(EXIT_SUCCESS);
	/* NOTREACHED */
}
