/*
  rc-depend
  rc service dependency and ordering
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

#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>

#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <utime.h>

#include "builtins.h"
#include "einfo.h"
#include "rc.h"
#include "rc-misc.h"

extern const char *applet;

RC_DEPTREE *
_rc_deptree_load(int force, int *regen) {
	int fd;
	int retval;
	int serrno = errno;
	int merrno;
	time_t t;
	char file[PATH_MAX];
	struct stat st;
	struct utimbuf ut;
	FILE *fp;

	t = 0;
	if (rc_deptree_update_needed(&t, file) || force != 0) {
		/* Test if we have permission to update the deptree */
		fd = open(RC_DEPTREE_CACHE, O_WRONLY);
		merrno = errno;
		errno = serrno;
		if (fd == -1 && merrno == EACCES)
			return rc_deptree_load();
		close(fd);

		if (regen)
			*regen = 1;
		ebegin("Caching service dependencies");
		retval = rc_deptree_update() ? 0 : -1;
		eend (retval, "Failed to update the dependency tree");

		if (retval == 0) {
			stat(RC_DEPTREE_CACHE, &st);
			if (st.st_mtime < t) {
				eerror("Clock skew detected with `%s'", file);
				eerrorn("Adjusting mtime of `" RC_DEPTREE_CACHE
				    "' to %s", ctime(&t));
				fp = fopen(RC_DEPTREE_SKEWED, "w");
				if (fp != NULL) {
					fprintf(fp, "%s\n", file);
					fclose(fp);
				}
				ut.actime = t;
				ut.modtime = t;
				utime(RC_DEPTREE_CACHE, &ut);
			} else {
				if (exists(RC_DEPTREE_SKEWED))
					unlink(RC_DEPTREE_SKEWED);
			}
		}
		if (force == -1 && regen != NULL)
			*regen = retval;
	}
	return rc_deptree_load();
}

#include "_usage.h"
#define getoptstring "aot:suTF:" getoptstring_COMMON
static const struct option longopts[] = {
	{ "starting", 0, NULL, 'a'},
	{ "stopping", 0, NULL, 'o'},
	{ "type",     1, NULL, 't'},
	{ "notrace",  0, NULL, 'T'},
	{ "strict",   0, NULL, 's'},
	{ "update",   0, NULL, 'u'},
	{ "deptree-file", 1, NULL, 'F'},
	longopts_COMMON
};
static const char * const longopts_help[] = {
	"Order services as if runlevel is starting",
	"Order services as if runlevel is stopping",
	"Type(s) of dependency to list",
	"Don't trace service dependencies",
	"Only use what is in the runlevels",
	"Force an update of the dependency tree",
	"File to load cached deptree from",
	longopts_help_COMMON
};
#include "_usage.c"

int
rc_depend(int argc, char **argv)
{
	RC_STRINGLIST *list;
	RC_STRINGLIST *types;
	RC_STRINGLIST *services;
	RC_STRINGLIST *depends;
	RC_STRING *s;
	RC_DEPTREE *deptree = NULL;
	int options = RC_DEP_TRACE, update = 0;
	bool first = true;
	char *runlevel = xstrdup(getenv("RC_RUNLEVEL"));
	int opt;
	char *token;
	char *deptree_file = NULL;

	types = rc_stringlist_new();
	while ((opt = getopt_long(argc, argv, getoptstring,
		    longopts, (int *) 0)) != -1)
	{
		switch (opt) {
		case 'a':
			options |= RC_DEP_START;
			break;
		case 'o':
			options |= RC_DEP_STOP;
			break;
		case 's':
			options |= RC_DEP_STRICT;
			break;
		case 't':
			while ((token = strsep(&optarg, ",")))
				rc_stringlist_add(types, token);
			break;
		case 'u':
			update = 1;
			break;
		case 'T':
			options &= RC_DEP_TRACE;
			break;
		case 'F':
			deptree_file = xstrdup(optarg);
			break;

		case_RC_COMMON_GETOPT
		}
	}

	if (deptree_file) {
		if (!(deptree = rc_deptree_load_file(deptree_file)))
			eerrorx("failed to load deptree");
	} else {
		if (!(deptree = _rc_deptree_load(update, NULL)))
			eerrorx("failed to load deptree");
	}

	if (!runlevel)
		runlevel = rc_runlevel_get();

	services = rc_stringlist_new();
	while (optind < argc) {
		list = rc_stringlist_new();
		rc_stringlist_add(list, argv[optind]);
		errno = 0;
		depends = rc_deptree_depends(deptree, NULL, list, runlevel, 0);
		if (!depends && errno == ENOENT)
			eerror("no dependency info for service `%s'",
			    argv[optind]);
		else
			rc_stringlist_add(services, argv[optind]);

		rc_stringlist_free(depends);
		rc_stringlist_free(list);
		optind++;
	}
	if (!TAILQ_FIRST(services)) {
		rc_stringlist_free(services);
		rc_stringlist_free(types);
		rc_deptree_free(deptree);
		free(runlevel);
		if (update)
			return EXIT_SUCCESS;
		eerrorx("no services specified");
	}

	/* If we don't have any types, then supply some defaults */
	if (!TAILQ_FIRST(types)) {
		rc_stringlist_add(types, "ineed");
		rc_stringlist_add(types, "iuse");
	}

	depends = rc_deptree_depends(deptree, types, services,
	    runlevel, options);

	if (TAILQ_FIRST(depends)) {
		TAILQ_FOREACH(s, depends, entries) {
			if (first)
				first = false;
			else
				printf (" ");
			printf ("%s", s->value);

		}
		printf ("\n");
	}

	rc_stringlist_free(types);
	rc_stringlist_free(services);
	rc_stringlist_free(depends);
	rc_deptree_free(deptree);
	free(runlevel);
	return EXIT_SUCCESS;
}
