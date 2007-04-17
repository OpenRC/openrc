/*
 * @file _usage.c
 * @brief standardize help/usage output across all our programs
 * @internal
 *
 * Copyright 2007 Gentoo Foundation
 * Released under the GPLv2
 */

#ifndef APPLET
# error you forgot to define APPLET
#endif

static void usage (int exit_status)
{
	int i;
	printf ("Usage: " APPLET " [options]\n\n");
	printf ("Options: [" getoptstring "]\n");
	for (i = 0; longopts[i].name; ++i)
		printf ("  -%c, --%s\n", longopts[i].val, longopts[i].name);
	exit (exit_status);
}

#define case_RC_COMMON_GETOPT \
	case 'h': usage (EXIT_SUCCESS); \
	default:  usage (EXIT_FAILURE);
