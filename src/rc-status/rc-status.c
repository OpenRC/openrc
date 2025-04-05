/*
 * rc-status.c
 * Display the status of the services in runlevels
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

#include <getopt.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <unistd.h>
#include <inttypes.h>
#include <errno.h>
#include <time.h>

#include "einfo.h"
#include "queue.h"
#include "rc.h"
#include "misc.h"
#include "_usage.h"
#include "helpers.h"

enum format_t {
	FORMAT_DEFAULT,
	FORMAT_INI,
};

const char *applet = NULL;
const char *extraopts = NULL;
const char getoptstring[] = "acf:i:lmrsSu" getoptstring_COMMON;
const struct option longopts[] = {
	{"all",         0, NULL, 'a'},
	{"crashed",     0, NULL, 'c'},
	{"format",     1, NULL, 'f'},
	{"in-state",   1, NULL, 'i'},
	{"list",        0, NULL, 'l'},
	{"manual",        0, NULL, 'm'},
	{"runlevel",    0, NULL, 'r'},
	{"servicelist", 0, NULL, 's'},
	{"supervised", 0, NULL, 'S'},
	{"unused",      0, NULL, 'u'},
	longopts_COMMON
};
const char * const longopts_help[] = {
	"Show services from all run levels",
	"Show crashed services",
	"format status to be parsable (currently arg must be ini)",
	"Show services which are in this state",
	"Show list of run levels",
	"Show manually started services",
	"Show the name of the current runlevel",
	"Show service list",
	"show supervised services",
	"Show services not assigned to any runlevel",
	longopts_help_COMMON
};
const char *usagestring = ""
	"Usage: rc-status [-C] [-f ini] [-i state] [runlevel]\n"
	"   or: rc-status [-C] [-f ini] [-a | -m | -S | -s | -u]\n"
	"   or: rc-status [-C] [-c | -l | -r]";

static RC_DEPTREE *deptree;
static RC_STRINGLIST *types;

static RC_STRINGLIST *levels, *services, *tmp, *alist;
static RC_STRINGLIST *sservices, *nservices, *needsme;

static void print_level(const char *prefix, const char *level,
		enum format_t format)
{
	switch (format) {
	case FORMAT_DEFAULT:
		if (prefix)
			printf("%s ", prefix);
		printf ("Runlevel: ");
		if (isatty(fileno(stdout)))
			printf("%s%s%s\n",
					ecolor(ECOLOR_HILITE), level, ecolor(ECOLOR_NORMAL));
		else
			printf("%s\n", level);
		break;
	case FORMAT_INI:
		printf("%s", "[");
		if (prefix)
			printf("%s ", prefix);
		printf("%s]\n", level);
		break;
	}
}

static char *get_uptime(const char *service)
{
	RC_SERVICE state = rc_service_state(service);
	char *start_count;
	char *start_time_string;
	time_t start_time;
	int64_t diff_days;
	int64_t diff_hours;
	int64_t diff_mins;
	int64_t diff_secs;
	char *uptime = NULL;

	if (state & RC_SERVICE_STARTED) {
		start_count = rc_service_value_get(service, "start_count");
		start_time_string = rc_service_value_get(service, "start_time");
		if (start_count && start_time_string) {
			start_time = to_time_t(start_time_string);
			diff_secs = (int64_t) difftime(time(NULL), start_time);
			diff_days = diff_secs / 86400;
			diff_secs = diff_secs % 86400;
			diff_hours = diff_secs / 3600;
			diff_secs = diff_secs % 3600;
			diff_mins = diff_secs / 60;
			diff_secs = diff_secs % 60;
			if (diff_days > 0)
				xasprintf(&uptime,
						"%"PRId64" day(s) %02"PRId64":%02"PRId64":%02"PRId64" (%s)",
						diff_days, diff_hours, diff_mins, diff_secs,
						start_count);
			else
				xasprintf(&uptime,
						"%02"PRId64":%02"PRId64":%02"PRId64" (%s)",
						diff_hours, diff_mins, diff_secs, start_count);
		}
		free(start_count);
		free(start_time_string);
	}
	return uptime;
}

static void print_service(const char *service, enum format_t format,
	RC_SERVICE accept, RC_SERVICE reject)
{
	char *status = NULL;
	char *uptime = NULL;
	char *child_pid = NULL;
	char *start_time = NULL;
	int cols;
	const char *c = ecolor(ECOLOR_GOOD);
	RC_SERVICE state = rc_service_state(service);
	ECOLOR color = ECOLOR_BAD;

	if (!(state & accept) || (state & reject))
		return;

	if (state & RC_SERVICE_STOPPING)
		xasprintf(&status, "stopping ");
	else if (state & RC_SERVICE_STARTING) {
		xasprintf(&status, "starting ");
		color = ECOLOR_WARN;
	} else if (state & RC_SERVICE_INACTIVE) {
		xasprintf(&status, "inactive ");
		color = ECOLOR_WARN;
	} else if (state & RC_SERVICE_STARTED) {
		errno = 0;
		if (rc_service_daemons_crashed(service) && errno != EACCES)
		{
			child_pid = rc_service_value_get(service, "child_pid");
			start_time = rc_service_value_get(service, "start_time");
			if (start_time && child_pid)
				xasprintf(&status, " unsupervised ");
			else
				xasprintf(&status, " crashed ");
			free(child_pid);
			free(start_time);
		} else {
			uptime = get_uptime(service);
			if (uptime) {
				xasprintf(&status, " started %s", uptime);
				free(uptime);
			} else
				xasprintf(&status, " started ");
			color = ECOLOR_GOOD;
		}
	} else if (state & RC_SERVICE_SCHEDULED) {
		xasprintf(&status, "scheduled");
		color = ECOLOR_WARN;
	} else if (state & RC_SERVICE_FAILED) {
		xasprintf(&status, "failed");
		color = ECOLOR_WARN;
	} else
		xasprintf(&status, " stopped ");

	errno = 0;
	switch (format) {
	case FORMAT_DEFAULT:
		cols =  printf(" %s", service);
		if (c && *c && isatty(fileno(stdout)))
			printf("\n");
		ebracket(cols, color, status);
		break;
	case FORMAT_INI:
		printf("%s = %s\n", service, status);
		break;
	}
	free(status);
}

static void print_services_in_state(const char *runlevel, RC_STRINGLIST *svcs,
		enum format_t format, RC_SERVICE accept, RC_SERVICE reject)
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
				print_service(s->value, format, accept, reject);
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
			print_service(s->value, format, accept, reject);
	}
	rc_stringlist_free(l);
}

static void print_services(const char *runlevel, RC_STRINGLIST *svcs,
	enum format_t format)
{
	print_services_in_state(runlevel, svcs, format, -1, 0);
}

static void print_stacked_services(const char *runlevel, enum format_t format,
	RC_SERVICE accept, RC_SERVICE reject)
{
	RC_STRINGLIST *stackedlevels, *servicelist;
	RC_STRING *stackedlevel;

	stackedlevels = rc_runlevel_stacks(runlevel);
	TAILQ_FOREACH(stackedlevel, stackedlevels, entries) {
		if (rc_stringlist_find(levels, stackedlevel->value) != NULL)
			continue;
		print_level("Stacked", stackedlevel->value, format);
		servicelist = rc_services_in_runlevel(stackedlevel->value);
		print_services_in_state(stackedlevel->value, servicelist,
			format, accept, reject);
		rc_stringlist_free(servicelist);
	}
	rc_stringlist_free(stackedlevels);
	stackedlevels = NULL;
}

int main(int argc, char **argv)
{
	RC_SERVICE state;
	RC_SERVICE accept = -1;
	RC_SERVICE reject = 0;
	RC_STRING *s, *l, *t, *level;
	const rc_service_state_name_t *it;
	enum format_t format = FORMAT_DEFAULT;
	bool levels_given = false;
	bool show_all = false;
	char *p, *runlevel = NULL;
	int opt, retval = 0;

	applet = basename_c(argv[0]);
	while ((opt = getopt_long(argc, argv, getoptstring, longopts,
				  (int *) 0)) != -1)
		switch (opt) {
		case 'a':
			show_all = true;
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
		case 'f':
			if (strcasecmp(optarg, "ini") == 0) {
				format = FORMAT_INI;
				setenv("EINFO_QUIET", "YES", 1);
			} else
				eerrorx("%s: invalid argument to --format switch", applet);
			break;
		case 'i':
			if (accept == (RC_SERVICE)-1)
				accept = 0;
			for (it = rc_service_state_names; it->name; ++it) {
				/*
				 * print_service() doesn't print wasinactive
				 */
				if (it->state == RC_SERVICE_WASINACTIVE)
					continue;
				if (strcmp(optarg, it->name) == 0) {
					state = it->state;
					break;
				}
			}
			if (it->name == NULL)
				eerrorx("%s: invalid argument to --in-state"
					" switch: %s", applet, optarg);
			accept |= state;
			/*
			 * for `-i started` we don't want to show crashed
			 * services unless `-i crashed` was used beforehand
			 */
			if (state == RC_SERVICE_STARTED && !(accept & RC_SERVICE_CRASHED)) {
				reject |= RC_SERVICE_CRASHED;
			} else if (state == RC_SERVICE_CRASHED) {
				/* clear it in case `-i started` set it beforehand */
				reject &= ~RC_SERVICE_CRASHED;
			}
			/* similar situation here with `failed` and `stopped` */
			if (state == RC_SERVICE_STOPPED && !(accept & RC_SERVICE_FAILED)) {
				reject |= RC_SERVICE_FAILED;
			} else if (state == RC_SERVICE_FAILED) {
				reject &= ~RC_SERVICE_FAILED;
			}
			break;
		case 'l':
			levels = rc_runlevel_list();
			TAILQ_FOREACH(l, levels, entries)
				printf("%s\n", l->value);
			goto exit;
			/* NOTREACHED */
		case 'm':
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
			TAILQ_FOREACH_SAFE(s, services, entries, t)
				if (rc_service_state(s->value) &
					(RC_SERVICE_STOPPED | RC_SERVICE_HOTPLUGGED)) {
					TAILQ_REMOVE(services, s, entries);
					free(s->value);
					free(s);
				}
			print_services(NULL, services, format);
			goto exit;
			/* NOTREACHED */
		case 'r':
			runlevel = rc_runlevel_get();
			printf("%s\n", runlevel);
			goto exit;
			/* NOTREACHED */
		case 'S':
			services = rc_services_in_state(RC_SERVICE_STARTED);
			TAILQ_FOREACH_SAFE(s, services, entries, t) {
				char *ret = rc_service_value_get(s->value, "child_pid");
				if (!ret) {
					TAILQ_REMOVE(services, s, entries);
					free(s->value);
					free(s);
				}
				free(ret);
			}
			print_services(NULL, services, format);
			goto exit;
			/* NOTREACHED */
		case 's':
			services = rc_services_in_runlevel(NULL);
			print_services(NULL, services, format);
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
			print_services(NULL, services, format);
			goto exit;
			/* NOTREACHED */

		case_RC_COMMON_GETOPT
		}

	if (!levels)
		levels = rc_stringlist_new();
	opt = (optind < argc) ? 0 : 1;
	while (optind < argc) {
		if (rc_runlevel_exists(argv[optind])) {
			levels_given = true;
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
		print_level(NULL, l->value, format);
		services = rc_services_in_runlevel(l->value);
		print_services_in_state(l->value, services, format, accept, reject);
		print_stacked_services(l->value, format, accept, reject);
		rc_stringlist_free(nservices);
		nservices = NULL;
		rc_stringlist_free(services);
		services = NULL;
	}

	if (show_all || !levels_given) {
		/* Show hotplugged services */
		print_level("Dynamic", "hotplugged", format);
		services = rc_services_in_state(RC_SERVICE_HOTPLUGGED);
		print_services_in_state(NULL, services, format, accept, reject);
		rc_stringlist_free(services);
		services = NULL;

		/* Show manually started and unassigned depended services */
		if (show_all) {
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
			rc_stringlist_free(nservices);
		}
		TAILQ_FOREACH_SAFE(s, services, entries, t) {
			state = rc_service_state(s->value);
			if ((rc_stringlist_find(sservices, s->value) ||
			    (state & ( RC_SERVICE_STOPPED | RC_SERVICE_HOTPLUGGED)))) {
				if (!(state & RC_SERVICE_FAILED)) {
					TAILQ_REMOVE(services, s, entries);
					free(s->value);
					free(s);
				}
			}
		}
		needsme = rc_stringlist_new();
		rc_stringlist_add(needsme, "needsme");
		rc_stringlist_add(needsme, "wantsme");
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

		print_level("Dynamic", "needed/wanted", format);
		print_services_in_state(NULL, nservices, format, accept, reject);
		print_level("Dynamic", "manual", format);
		print_services_in_state(NULL, services, format, accept, reject);
	}

exit:
	free(runlevel);
	rc_stringlist_free(alist);
	rc_stringlist_free(needsme);
	rc_stringlist_free(sservices);
	rc_stringlist_free(nservices);
	rc_stringlist_free(services);
	rc_stringlist_free(types);
	rc_stringlist_free(levels);
	rc_deptree_free(deptree);

	return retval;
}
