/*!
 * @file _usage.c
 * @brief standardize help/usage output across all our programs
 * @internal
 *
 * Copyright 2007 Gentoo Foundation
 * Released under the GPLv2
 */

static void usage (int exit_status)
{
	int i;
	printf ("Usage: " APPLET " [options] ");
#ifdef extraopts
	printf (extraopts);
#endif
	printf ("\n\nOptions: [" getoptstring "]\n");
	for (i = 0; longopts[i].name; ++i)
		printf ("  -%c, --%-15s %s\n", longopts[i].val, longopts[i].name,
		        longopts_help[i]);
	exit (exit_status);
}

