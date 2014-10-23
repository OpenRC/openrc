/*
  rc-service.c
  Finds all OpenRC services
*/

/*
 * Copyright (c) 2008 Roy Marples <roy@marples.name>
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

extern char *applet;

#include "_usage.h"
#define usagestring ""							\
	"Usage: rc-service [options] [-i] <service> <cmd>...\n"		\
	"   or: rc-service [options] -e <service>\n"			\
	"   or: rc-service [options] -l\n"				\
	"   or: rc-service [options] -r <service>"
#define getoptstring "e:ilr:" getoptstring_COMMON
static const struct option longopts[] = {
	{ "exists",   1, NULL, 'e' },
	{ "ifexists", 0, NULL, 'i' },
	{ "list",     0, NULL, 'l' },
	{ "resolve",  1, NULL, 'r' },
	longopts_COMMON
};
static const char * const longopts_help[] = {
	"tests if the service exists or not",
	"if the service exists then run the command",
	"list all available services",
	"resolve the service name to an init script",
	longopts_help_COMMON
};
#include "_usage.c"

int
rc_service(int argc, char **argv)
{
	int opt;
	char *service;
	RC_STRINGLIST *list;
	RC_STRING *s;
	bool if_exists = false;

	/* Ensure that we are only quiet when explicitly told to be */
	unsetenv("EINFO_QUIET");

	while ((opt = getopt_long(argc, argv, getoptstring,
		    longopts, (int *) 0)) != -1)
	{
		switch (opt) {
		case 'e':
			service = rc_service_resolve(optarg);
			opt = service ? EXIT_SUCCESS : EXIT_FAILURE;
#ifdef DEBUG_MEMORY
			free(service);
#endif
			return opt;
			/* NOTREACHED */
		case 'i':
			if_exists = true;
			break;
		case 'l':
			list = rc_services_in_runlevel(NULL);
			if (TAILQ_FIRST(list) == NULL)
				return EXIT_FAILURE;
			rc_stringlist_sort(&list);
			TAILQ_FOREACH(s, list, entries)
			    printf("%s\n", s->value);
#ifdef DEBUG_MEMORY
			rc_stringlist_free(list);
#endif
			return EXIT_SUCCESS;
			/* NOTREACHED */
		case 'r':
			service = rc_service_resolve(optarg);
			if (service == NULL)
				return EXIT_FAILURE;
			printf("%s\n", service);
#ifdef DEBUG_MEMORY
			free(service);
#endif
			return EXIT_SUCCESS;
			/* NOTREACHED */

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
	*argv = service;
	execv(*argv, argv);
	eerrorx("%s: %s", applet, strerror(errno));
	/* NOTREACHED */
}
