/*!
 * @file _usage.c
 * @brief standardize help/usage output across all our programs
 * @internal
 *
 * Copyright 2007 Gentoo Foundation
 * Released under the GPLv2
 */

__attribute__ ((__noreturn__))
static void usage (int exit_status)
{
	const char * const has_arg[] = { "", "<arg>", "[arg]" };
	int i;
	printf ("Usage: " APPLET " [options] ");
#ifdef extraopts
	printf (extraopts);
#endif
	printf ("\n\nOptions: [" getoptstring "]\n");
	for (i = 0; longopts[i].name; ++i) {
		int len = printf ("  -%c, --%s %s", longopts[i].val, longopts[i].name,
		                  has_arg[longopts[i].has_arg]);
		while (++len < 30)
			printf (" ");
		puts (longopts_help[i]);
	}
	exit (exit_status);
}

