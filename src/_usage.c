/* 
 * Copyright 2007 Gentoo Foundation
 * Copyright 2007 Roy Marples
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

		char *lo = xstrdup (longopts_help[i]);
		char *p = lo;
		char *token;

		while ((token = strsep (&p, "\n"))) {
			while (++len < 37)
				printf (" ");
			puts (token);
			len = 0;
		}
		free (lo);
	}
	exit (exit_status);
}

