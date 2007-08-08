/*
   rc-update
   Manage init scripts and runlevels
   Copyright 2007 Gentoo Foundation
   Released under the GPLv2
   */

#define APPLET "rc-update"

#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "builtins.h"
#include "einfo.h"
#include "rc.h"
#include "rc-misc.h"
#include "strlist.h"

static char *applet = NULL;

static void usage (void )
{
	printf (
			"usage: %s -a|add script runlevel1 [runlevel2 ...]\n"
			"       %s -d|del script [runlevel1 ...]\n"
			"       %s -s|show [-v|--verbose] [runlevel1 ...]\n"
			"       %s -h|help\n"
			"\n"
			"examples:\n"
			"       # %s add net.eth0 default\n"
			"       Adds the net.eth0 script (in /etc/init.d) to the `default' runlevel.\n"
			"\n"
			"       # %s del sysklogd\n"
			"       Deletes the sysklogd script from all runlevels.  The original script\n"
			"       is not deleted, just any symlinks to the script in /etc/runlevels/*.\n"
			"\n"
			"       # %s del net.eth2 default wumpus\n"
			"       Delete the net.eth2 script from the default and wumpus runlevels.\n"
			"       All other runlevels are unaffected.  Again, the net.eth2 script\n"
			"       residing in /etc/init.d is not deleted, just any symlinks in\n"
			"       /etc/runlevels/default and /etc/runlevels/wumpus.\n"
			"\n"
			"       # %s show\n"
			"       Show all enabled scripts and list at which runlevels they will\n"
			"       execute.  Run with --verbose to see all available scripts.\n",
		applet, applet, applet, applet, applet, applet, applet, applet);
}

static bool add (const char *runlevel, const char *service)
{
	bool retval = false;

	if (! rc_service_exists (service))
		eerror ("service `%s' does not exist", service);
	else if (! rc_runlevel_exists (runlevel))
		eerror ("runlevel `%s' does not exist", runlevel);
	else if (rc_service_in_runlevel (service, runlevel)) {
		ewarn ("%s already installed in runlevel `%s'; skipping",
			   service, runlevel);
		retval = true;
	} else if (rc_service_add (runlevel, service)) {
		einfo ("%s added to runlevel %s", service, runlevel);
		retval = true;
	} else
		eerror ("%s: failed to add service `%s' to runlevel `%s': %s",
				applet, service, runlevel, strerror (errno));

	return (retval);
}

static bool delete (const char *runlevel, const char *service)
{
	bool retval = false;

	if (rc_service_in_runlevel (service, runlevel))	{
		if (rc_service_delete (runlevel, service)) {
			einfo ("%s removed from runlevel %s", service, runlevel);
			retval = true;
		} else
			eerror ("%s: failed to remove service `%s' from runlevel `%s': %s",
					applet, service, runlevel, strerror (errno));
	} else if (! rc_service_exists (service))
		eerror ("service `%s' does not exist", service);
	else if (! rc_runlevel_exists (runlevel))
		eerror ("runlevel `%s' does not exist", runlevel);
	else
		retval = true;
	return (retval);
}

int rc_update (int argc, char **argv)
{
	int i;
	int j;
	char *service;
	char **runlevels = NULL;
	char *runlevel;
	bool doadd;
	int retval;

	applet = argv[0];
	if (argc < 2 ||
		strcmp (argv[1], "show") == 0 ||
		strcmp (argv[1], "-s") == 0)
	{
		bool verbose = false;
		char **services = rc_services_in_runlevel (NULL);

		for (i = 2; i < argc; i++) {
			if (strcmp (argv[i], "--verbose") == 0 ||
				strcmp (argv[i], "-v") == 0)
				verbose = true;
			else
				runlevels = rc_strlist_add (runlevels, argv[i]);
		}

		if (! runlevels)
			runlevels = rc_get_runlevels ();

		STRLIST_FOREACH (services, service, i) {
			char **in = NULL;
			bool inone = false;

			STRLIST_FOREACH (runlevels, runlevel, j) {
				if (rc_service_in_runlevel (service, runlevel)) {
					in = rc_strlist_add (in, runlevel);
					inone = true;
				} else {
					char buffer[PATH_MAX];
					memset (buffer, ' ', strlen (runlevel));
					buffer[strlen (runlevel)] = 0;
					in = rc_strlist_add (in, buffer);
				}
			}

			if (! inone && ! verbose)
				continue;

			printf (" %20s |", service);
			STRLIST_FOREACH (in, runlevel, j)
				printf (" %s", runlevel);
			printf ("\n");
			rc_strlist_free (in);
		}

		rc_strlist_free (runlevels);
		rc_strlist_free (services);
		return (EXIT_SUCCESS);
	} else if (argc > 1 &&
			   (strcmp (argv[1], "help") == 0 ||
				strcmp (argv[1], "--help") == 0 ||
				strcmp (argv[1], "-h") == 0))
	{
		usage ();
		return (EXIT_SUCCESS);
	}

	if (geteuid () != 0)
		eerrorx ("%s: must be root to add or delete services from runlevels",
				 applet);

	if (! (service = argv[2]))
		eerrorx ("%s: no service specified", applet);

	if (strcmp (argv[1], "add") == 0 ||
		strcmp (argv[1], "-a") == 0)
		doadd = true;
	else if (strcmp (argv[1], "delete") == 0 ||
		strcmp (argv[1], "del") == 0 ||
		strcmp (argv[1], "-d") == 0)
		doadd = false;
	else
		eerrorx ("%s: unknown command `%s'", applet, argv[1]);

	for (i = 3; i < argc; i++)
		runlevels = rc_strlist_add (runlevels, argv[i]);

	if (! runlevels)
		runlevels = rc_strlist_add (runlevels, rc_get_runlevel ()); 

	retval = EXIT_SUCCESS;
	STRLIST_FOREACH (runlevels, runlevel, i) {
		if (doadd) {
			if (! add (runlevel, service))
				retval = EXIT_FAILURE;
		} else {
			if (! delete (runlevel, service))
				retval = EXIT_FAILURE;
		}
	}

	rc_strlist_free (runlevels);

	return (retval);
}
