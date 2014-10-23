/*
   rc-status
   Display the status of the services in runlevels
   */

/*
 * Copyright (c) 2007-2009 Roy Marples <roy@marples.name>
 *
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
#include "queue.h"
#include "rc.h"
#include "rc-misc.h"

extern const char *applet;
static bool test_crashed = false;
static RC_DEPTREE *deptree;
static RC_STRINGLIST *types;

static RC_STRINGLIST *levels, *services, *tmp, *alist;
static RC_STRINGLIST *sservices, *nservices, *needsme;

bool
_rc_can_find_pids(void)
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

static void
print_level(const char *prefix, const char *level)
{
	if (prefix)
		printf("%s ", prefix);
	printf ("Runlevel: ");
	if (isatty(fileno(stdout)))
		printf("%s%s%s\n",
		       ecolor(ECOLOR_HILITE),
		       level,
		       ecolor(ECOLOR_NORMAL));
	else
		printf("%s\n", level);
}

static void
print_service(const char *service)
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
		errno = 0;
		if (test_crashed &&
		    rc_service_daemons_crashed(service) &&
		    errno != EACCES)
		{
			snprintf(status, sizeof(status), " crashed ");
		} else {
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

static void
print_services(const char *runlevel, RC_STRINGLIST *svcs)
{
	RC_STRINGLIST *l = NULL;
	RC_STRING *s;
	char *r = NULL;

	if (!svcs)
		return;
	if (!deptree)
		deptree = _rc_deptree_load(0, NULL);
	if (!deptree) {
		TAILQ_FOREACH(s, svcs, entries)
			if (!runlevel ||
			    rc_service_in_runlevel(s->value, runlevel))
				print_service(s->value);
		return;
	}
	if (!types) {
		types = rc_stringlist_new();
		rc_stringlist_add(types, "ineed");
		rc_stringlist_add(types, "iuse");
		rc_stringlist_add(types, "iafter");
	}
	if (!runlevel)
		r = rc_runlevel_get();
	l = rc_deptree_depends(deptree, types, svcs, r ? r : runlevel,
			       RC_DEP_STRICT | RC_DEP_TRACE | RC_DEP_START);
	free(r);
	if (!l)
		return;
	TAILQ_FOREACH(s, l, entries) {
		if (!rc_stringlist_find(svcs, s->value))
			continue;
		if (!runlevel || rc_service_in_runlevel(s->value, runlevel))
			print_service(s->value);
	}
	rc_stringlist_free(l);
}

static void
print_stacked_services(const char *runlevel)
{
	RC_STRINGLIST *stackedlevels, *servicelist;
	RC_STRING *stackedlevel;

	stackedlevels = rc_runlevel_stacks(runlevel);
	TAILQ_FOREACH(stackedlevel, stackedlevels, entries) {
		if (rc_stringlist_find(levels, stackedlevel->value) != NULL)
			continue;
		print_level("Stacked", stackedlevel->value);
		servicelist = rc_services_in_runlevel(stackedlevel->value);
		print_services(stackedlevel->value, servicelist);
		rc_stringlist_free(servicelist);
	}
	rc_stringlist_free(stackedlevels);
	stackedlevels = NULL;
}

#include "_usage.h"
#define usagestring ""						\
	"Usage: rc-status [options] <runlevel>...\n"		\
	"   or: rc-status [options] [-a | -c | -l | -r | -s | -u]"
#define getoptstring "aclrsu" getoptstring_COMMON
static const struct option longopts[] = {
	{"all",         0, NULL, 'a'},
	{"crashed",     0, NULL, 'c'},
	{"list",        0, NULL, 'l'},
	{"runlevel",    0, NULL, 'r'},
	{"servicelist", 0, NULL, 's'},
	{"unused",      0, NULL, 'u'},
	longopts_COMMON
};
static const char * const longopts_help[] = {
	"Show services from all run levels",
	"Show crashed services",
	"Show list of run levels",
	"Show the name of the current runlevel",
	"Show service list",
	"Show services not assigned to any runlevel",
	longopts_help_COMMON
};
#include "_usage.c"

int
rc_status(int argc, char **argv)
{
    RC_STRING *s, *l, *t, *level;

	char *p, *runlevel = NULL;
	int opt, aflag = 0, retval = 0;

	test_crashed = _rc_can_find_pids();

	while ((opt = getopt_long(argc, argv, getoptstring, longopts,
				  (int *) 0)) != -1)
		switch (opt) {
		case 'a':
			aflag++;
			levels = rc_runlevel_list();
			break;
		case 'c':
			services = rc_services_in_state(RC_SERVICE_STARTED);
			retval = 1;
			TAILQ_FOREACH(s, services, entries)
				if (rc_service_daemons_crashed(s->value)) {
					printf("%s\n", s->value);
					retval = 0;
				}
			goto exit;
			/* NOTREACHED */
		case 'l':
			levels = rc_runlevel_list();
			TAILQ_FOREACH(l, levels, entries)
				printf("%s\n", l->value);
			goto exit;
		case 'r':
			runlevel = rc_runlevel_get();
			printf("%s\n", runlevel);
			goto exit;
			/* NOTREACHED */
		case 's':
			services = rc_services_in_runlevel(NULL);
			print_services(NULL, services);
			goto exit;
			/* NOTREACHED */
		case 'u':
			services = rc_services_in_runlevel(NULL);
			levels = rc_runlevel_list();
			TAILQ_FOREACH_SAFE(s, services, entries, t) {
				TAILQ_FOREACH(l, levels, entries)
					if (rc_service_in_runlevel(s->value, l->value)) {
						TAILQ_REMOVE(services, s, entries);
						free(s->value);
						free(s);
						break;
					}
			}
			print_services(NULL, services);
			goto exit;
			/* NOTREACHED */

		case_RC_COMMON_GETOPT
		}

	if (!levels)
		levels = rc_stringlist_new();
	opt = (optind < argc) ? 0 : 1;
	while (optind < argc) {
		if (rc_runlevel_exists(argv[optind])) {
			rc_stringlist_add(levels, argv[optind++]);
			opt++;
		} else
			eerror("runlevel `%s' does not exist", argv[optind++]);
	}
	if (opt == 0)
		exit(EXIT_FAILURE);
	if (!TAILQ_FIRST(levels)) {
		runlevel = rc_runlevel_get();
		rc_stringlist_add(levels, runlevel);
	}

	/* Output the services in the order in which they would start */
	deptree = _rc_deptree_load(0, NULL);

	TAILQ_FOREACH(l, levels, entries) {
		print_level(NULL, l->value);
		services = rc_services_in_runlevel(l->value);
		print_services(l->value, services);
		print_stacked_services(l->value);
		rc_stringlist_free(nservices);
		nservices = NULL;
		rc_stringlist_free(services);
		services = NULL;
	}

	if (aflag || argc < 2) {
		/* Show hotplugged services */
		print_level("Dynamic", "hotplugged");
		services = rc_services_in_state(RC_SERVICE_HOTPLUGGED);
		print_services(NULL, services);
		rc_stringlist_free(services);
		services = NULL;

		/* Show manually started and unassigned depended services */
		if (aflag) {
			rc_stringlist_free(levels);
			levels = rc_stringlist_new();
			if (!runlevel)
				runlevel = rc_runlevel_get();
			rc_stringlist_add(levels, runlevel);
		}
		rc_stringlist_add(levels, RC_LEVEL_SYSINIT);
		rc_stringlist_add(levels, RC_LEVEL_BOOT);
		services = rc_services_in_runlevel(NULL);
		sservices = rc_stringlist_new();
		TAILQ_FOREACH(l, levels, entries) {
			nservices = rc_services_in_runlevel_stacked(l->value);
			TAILQ_CONCAT(sservices, nservices, entries);
			free(nservices);
		}
		TAILQ_FOREACH_SAFE(s, services, entries, t) {
			if ((rc_stringlist_find(sservices, s->value) ||
			    (rc_service_state(s->value) & ( RC_SERVICE_STOPPED | RC_SERVICE_HOTPLUGGED)))) {
				TAILQ_REMOVE(services, s, entries);
				free(s->value);
				free(s);
			}
		}
		needsme = rc_stringlist_new();
		rc_stringlist_add(needsme, "needsme");
		nservices = rc_stringlist_new();
		alist = rc_stringlist_new();
		l = rc_stringlist_add(alist, "");
		p = l->value;
		TAILQ_FOREACH(level, levels, entries) {
			TAILQ_FOREACH_SAFE(s, services, entries, t) {
				l->value = s->value;
				setenv("RC_SVCNAME", l->value, 1);
				tmp = rc_deptree_depends(deptree, needsme, alist, level->value, RC_DEP_TRACE);
				if (TAILQ_FIRST(tmp)) {
					TAILQ_REMOVE(services, s, entries);
					TAILQ_INSERT_TAIL(nservices, s, entries);
				}
				rc_stringlist_free(tmp);
			}
		}
		l->value = p;
		/*
		 * we are unsetting RC_SVCNAME because last loaded service will not
		 * be added to the list
		 */
		unsetenv("RC_SVCNAME");
		print_level("Dynamic", "needed");
		print_services(NULL, nservices);
		print_level("Dynamic", "manual");
		print_services(NULL, services);
	}

exit:
	free(runlevel);
#ifdef DEBUG_MEMORY
	rc_stringlist_free(alist);
	rc_stringlist_free(needsme);
	rc_stringlist_free(sservices);
	rc_stringlist_free(nservices);
	rc_stringlist_free(services);
	rc_stringlist_free(types);
	rc_stringlist_free(levels);
	rc_deptree_free(deptree);
#endif

	return retval;
}
