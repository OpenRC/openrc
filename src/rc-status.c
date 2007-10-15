/*
   rc-status
   Display the status of the services in runlevels
   Copyright 2007 Gentoo Foundation
   Released under the GPLv2
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
#include "strlist.h"

#define APPLET "rc-status"

static const char *types_nua[] = { "ineed", "iuse", "iafter", NULL };

static void print_level (char *level)
{
	printf ("Runlevel: %s%s%s\n",
			ecolor (ECOLOR_HILITE),
			level,
			ecolor (ECOLOR_NORMAL));
}

static void print_service (char *service)
{
	char status[10];
	int cols =  printf (" %s", service);
	rc_service_state_t state = rc_service_state (service);
	einfo_color_t color = ECOLOR_BAD;

	if (state & RC_SERVICE_STOPPING)
		snprintf (status, sizeof (status), "stopping ");
	else if (state & RC_SERVICE_STARTING) {
		snprintf (status, sizeof (status), "starting ");
		color = ECOLOR_WARN;
	} else if (state & RC_SERVICE_INACTIVE) {
		snprintf (status, sizeof (status), "inactive ");
		color = ECOLOR_WARN;
	} else if (state & RC_SERVICE_STARTED) {
		if (geteuid () == 0 && rc_service_daemons_crashed (service))
			snprintf (status, sizeof (status), " crashed ");
		else {
			snprintf (status, sizeof (status), " started ");
			color = ECOLOR_GOOD;
		}
	} else if (state & RC_SERVICE_SCHEDULED) {
		snprintf (status, sizeof (status), "scheduled");
		color = ECOLOR_WARN;
	} else
		snprintf (status, sizeof (status), " stopped ");

	if (isatty (fileno (stdout)) && ! rc_env_bool ("RC_NOCOLOR"))
		printf ("\n");
	ebracket (cols, color, status);
}

#include "_usage.h"
#define extraopts "[runlevel1] [runlevel2] ..."
#define getoptstring "alsu" getoptstring_COMMON
static const struct option longopts[] = {
	{"all",         0, NULL, 'a'},
	{"list",        0, NULL, 'l'},
	{"servicelist", 0, NULL, 's'},
	{"unused",      0, NULL, 'u'},
	longopts_COMMON
};
static const char * const longopts_help[] = {
	"Show services from all run levels",
	"Show list of run levels",
	"Show service list",
	"Show services not assigned to any runlevel",
	longopts_help_COMMON
};
#include "_usage.c"

int rc_status (int argc, char **argv)
{
	rc_depinfo_t *deptree = NULL;
	char **levels = NULL;
	char **services = NULL;
	char **ordered = NULL;
	char *level;
	char *service;
	int opt;
	int i;
	int j;
	int depopts = RC_DEP_STRICT | RC_DEP_START | RC_DEP_TRACE;

	while ((opt = getopt_long(argc, argv, getoptstring, longopts, (int *) 0)) != -1)
		switch (opt) {
			case 'a':
				levels = rc_runlevel_list ();
				break;
			case 'l':
				levels = rc_runlevel_list ();
				STRLIST_FOREACH (levels, level, i)
					printf ("%s\n", level);
				rc_strlist_free (levels);
				exit (EXIT_SUCCESS);
			case 's':
				services = rc_services_in_runlevel (NULL);
				STRLIST_FOREACH (services, service, i)
					print_service (service);
				rc_strlist_free (services);
				exit (EXIT_SUCCESS);
			case 'u':
				services = rc_services_in_runlevel (NULL);
				levels = rc_runlevel_list ();
				STRLIST_FOREACH (services, service, i) {
					bool found = false;
					STRLIST_FOREACH (levels, level, j)
						if (rc_service_in_runlevel (service, level)) {
							found = true;
							break;
						}
					if (! found)
						print_service (service);
				}
				rc_strlist_free (levels);
				rc_strlist_free (services);
				exit (EXIT_SUCCESS);

			case_RC_COMMON_GETOPT
		}

	while (optind < argc)
		rc_strlist_add (&levels, argv[optind++]);

	if (! levels) {
		level = rc_runlevel_get ();
		rc_strlist_add (&levels, level);
		free (level);
	}

	/* Output the services in the order in which they would start */
	deptree = rc_deptree_load ();
	STRLIST_FOREACH (levels, level, i) {
		print_level (level);
		services = rc_services_in_runlevel (level);
		if (deptree) {
			ordered = rc_deptree_depends (deptree, types_nua,
										  (const char **) services,
										  level, depopts);
			rc_strlist_free (services);
			services = ordered;
			ordered = NULL;
		}
		STRLIST_FOREACH (services, service, j)
			if (rc_service_in_runlevel (service, level))
				print_service (service);
		rc_strlist_free (services);
	}

	rc_strlist_free (levels);
	rc_deptree_free (deptree);

	return (EXIT_SUCCESS);
}
