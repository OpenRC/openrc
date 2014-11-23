/*
  mountinfo.c
  Obtains information about mounted filesystems.
*/

/*
 * Copyright (c) 2007-2008 Roy Marples <roy@marples.name>
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

#include <sys/types.h>
#include <sys/param.h>

#if defined(__DragonFly__) || defined(__FreeBSD__)
#  include <sys/ucred.h>
#  include <sys/mount.h>
#  define F_FLAGS f_flags
#elif defined(BSD) && !defined(__GNU__)
#  include <sys/statvfs.h>
#  define statfs statvfs
#  define F_FLAGS f_flag
#elif defined (__linux__) || (defined(__FreeBSD_kernel__) && \
		defined(__GLIBC__)) || defined(__GNU__)
#  include <mntent.h>
#endif

#include <errno.h>
#include <getopt.h>
#include <limits.h>
#include <regex.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "builtins.h"
#include "einfo.h"
#include "queue.h"
#include "rc.h"
#include "rc-misc.h"

extern const char *applet;

typedef enum {
	mount_from,
	mount_to,
	mount_fstype,
	mount_options
} mount_type;

typedef enum {
	net_ignore,
	net_yes,
	net_no
} net_opts;

struct args {
	regex_t *node_regex;
	regex_t *skip_node_regex;
	regex_t *fstype_regex;
	regex_t *skip_fstype_regex;
	regex_t *options_regex;
	regex_t *skip_options_regex;
	RC_STRINGLIST *mounts;
	mount_type mount_type;
	net_opts netdev;
};

static int
process_mount(RC_STRINGLIST *list, struct args *args,
    char *from, char *to, char *fstype, char *options,
    int netdev)
{
	char *p;
	RC_STRING *s;

	errno = ENOENT;

#ifdef __linux__
	/* Skip the really silly rootfs */
	if (strcmp(fstype, "rootfs") == 0)
		return -1;
#endif

	if (args->netdev == net_yes &&
	    (netdev != -1 || TAILQ_FIRST(args->mounts)))
	{
		if (netdev != 0)
			return 1;
	} else if (args->netdev == net_no &&
	    (netdev != -1 || TAILQ_FIRST(args->mounts)))
	{
		if (netdev != 1)
			return 1;
	} else {
		if (args->node_regex &&
		    regexec(args->node_regex, from, 0, NULL, 0) != 0)
			return 1;
		if (args->skip_node_regex &&
		    regexec(args->skip_node_regex, from, 0, NULL, 0) == 0)
			return 1;

		if (args->fstype_regex &&
		    regexec(args->fstype_regex, fstype, 0, NULL, 0) != 0)
			return -1;
		if (args->skip_fstype_regex &&
		    regexec(args->skip_fstype_regex, fstype, 0, NULL, 0) == 0)
			return -1;

		if (args->options_regex &&
		    regexec(args->options_regex, options, 0, NULL, 0) != 0)
			return -1;
		if (args->skip_options_regex &&
		    regexec(args->skip_options_regex, options, 0, NULL, 0) == 0)
			return -1;
	}

	if (TAILQ_FIRST(args->mounts)) {
		TAILQ_FOREACH(s, args->mounts, entries)
		    if (strcmp(s->value, to) == 0)
			    break;
		if (! s)
			return -1;
	}

	switch (args->mount_type) {
	case mount_from:
		p = from;
		break;
	case mount_to:
		p = to;
		break;
	case mount_fstype:
		p = fstype;
		break;
	case mount_options:
		p = options;
		break;
	default:
		p = NULL;
		errno = EINVAL;
		break;
	}

	if (p) {
		errno = 0;
		rc_stringlist_add(list, p);
		return 0;
	}

	return -1;
}

#if defined(BSD) && !defined(__GNU__)

/* Translate the mounted options to english
 * This is taken directly from FreeBSD mount.c */
static struct opt {
	int o_opt;
	const char *o_name;
} optnames[] = {
	{ MNT_ASYNC,        "asynchronous" },
	{ MNT_EXPORTED,     "NFS exported" },
	{ MNT_LOCAL,        "local" },
	{ MNT_NOATIME,      "noatime" },
	{ MNT_NOEXEC,       "noexec" },
	{ MNT_NOSUID,       "nosuid" },
#ifdef MNT_NOSYMFOLLOW
	{ MNT_NOSYMFOLLOW,  "nosymfollow" },
#endif
	{ MNT_QUOTA,        "with quotas" },
	{ MNT_RDONLY,       "read-only" },
	{ MNT_SYNCHRONOUS,  "synchronous" },
	{ MNT_UNION,        "union" },
#ifdef MNT_NOCLUSTERR
	{ MNT_NOCLUSTERR,   "noclusterr" },
#endif
#ifdef MNT_NOCLUSTERW
	{ MNT_NOCLUSTERW,   "noclusterw" },
#endif
#ifdef MNT_SUIDDIR
	{ MNT_SUIDDIR,      "suiddir" },
#endif
	{ MNT_SOFTDEP,      "soft-updates" },
#ifdef MNT_MULTILABEL
	{ MNT_MULTILABEL,   "multilabel" },
#endif
#ifdef MNT_ACLS
	{ MNT_ACLS,         "acls" },
#endif
#ifdef MNT_GJOURNAL
	{ MNT_GJOURNAL,     "gjournal" },
#endif
	{ 0, NULL }
};

static RC_STRINGLIST *
find_mounts(struct args *args)
{
	struct statfs *mnts;
	int nmnts;
	int i;
	RC_STRINGLIST *list;
	char *options = NULL;
	uint64_t flags;
	struct opt *o;
	int netdev;
	char *tmp;
	size_t l;

	if ((nmnts = getmntinfo(&mnts, MNT_NOWAIT)) == 0)
		eerrorx("getmntinfo: %s", strerror (errno));

	list = rc_stringlist_new();
	for (i = 0; i < nmnts; i++) {
		netdev = 0;
		flags = mnts[i].F_FLAGS & MNT_VISFLAGMASK;
		for (o = optnames; flags && o->o_opt; o++) {
			if (flags & o->o_opt) {
				if (o->o_opt == MNT_LOCAL)
					netdev = 1;
				if (! options)
					options = xstrdup(o->o_name);
				else {
					l = strlen(options) +
					    strlen(o->o_name) + 2;
					tmp = xmalloc(sizeof (char) * l);
					snprintf(tmp, l, "%s,%s", options,
					    o->o_name);
					free(options);
					options = tmp;
				}
			}
			flags &= ~o->o_opt;
		}

		process_mount(list, args,
		    mnts[i].f_mntfromname,
		    mnts[i].f_mntonname,
		    mnts[i].f_fstypename,
		    options,
		    netdev);

		free(options);
		options = NULL;
	}

	return list;
}

#elif defined (__linux__) || (defined (__FreeBSD_kernel__) && \
		defined(__GLIBC__))
static struct mntent *
getmntfile(const char *file)
{
	struct mntent *ent = NULL;
	FILE *fp;

	fp = setmntent("/etc/fstab", "r");
	while ((ent = getmntent(fp)))
		if (strcmp(file, ent->mnt_dir) == 0)
			break;
	endmntent(fp);

	return ent;
}

static RC_STRINGLIST *
find_mounts(struct args *args)
{
	FILE *fp;
	char *buffer;
	char *p;
	char *from;
	char *to;
	char *fst;
	char *opts;
	struct mntent *ent;
	int netdev;
	RC_STRINGLIST *list;

	if ((fp = fopen("/proc/mounts", "r")) == NULL)
		eerrorx("getmntinfo: %s", strerror(errno));

	list = rc_stringlist_new();

	buffer = xmalloc(sizeof(char) * PATH_MAX * 3);
	while (fgets(buffer, PATH_MAX * 3, fp)) {
		netdev = -1;
		p = buffer;
		from = strsep(&p, " ");
		to = strsep(&p, " ");
		fst = strsep(&p, " ");
		opts = strsep(&p, " ");

		if ((ent = getmntfile(to))) {
			if (strstr(ent->mnt_opts, "_netdev"))
				netdev = 0;
		}

		process_mount(list, args, from, to, fst, opts, netdev);
	}
	free(buffer);
	fclose(fp);

	return list;
}

#else
#  error "Operating system not supported!"
#endif

static regex_t *
get_regex(const char *string)
{
	regex_t *reg = xmalloc(sizeof (*reg));
	int result;
	char buffer[256];

	if ((result = regcomp(reg, string, REG_EXTENDED | REG_NOSUB)) != 0)
	{
		regerror(result, reg, buffer, sizeof(buffer));
		eerrorx("%s: invalid regex `%s'", applet, buffer);
	}

	return reg;
}

#include "_usage.h"
#define extraopts "[mount1] [mount2] ..."
#define getoptstring "f:F:n:N:o:O:p:P:ist" getoptstring_COMMON
static const struct option longopts[] = {
	{ "fstype-regex",        1, NULL, 'f'},
	{ "skip-fstype-regex",   1, NULL, 'F'},
	{ "node-regex",          1, NULL, 'n'},
	{ "skip-node-regex",     1, NULL, 'N'},
	{ "options-regex",       1, NULL, 'o'},
	{ "skip-options-regex",  1, NULL, 'O'},
	{ "point-regex",         1, NULL, 'p'},
	{ "skip-point-regex",    1, NULL, 'P'},
	{ "options",             0, NULL, 'i'},
	{ "fstype",              0, NULL, 's'},
	{ "node",                0, NULL, 't'},
	{ "netdev",              0, NULL, 'e'},
	{ "nonetdev",            0, NULL, 'E'},
	longopts_COMMON
};
static const char * const longopts_help[] = {
	"fstype regex to find",
	"fstype regex to skip",
	"node regex to find",
	"node regex to skip",
	"options regex to find",
	"options regex to skip",
	"point regex to find",
	"point regex to skip",
	"print options",
	"print fstype",
	"print node",
	"is it a network device",
	"is it not a network device",
	longopts_help_COMMON
};
#include "_usage.c"

int
mountinfo(int argc, char **argv)
{
	struct args args;
	regex_t *point_regex = NULL;
	regex_t *skip_point_regex = NULL;
	RC_STRINGLIST *nodes;
	RC_STRING *s;
	char real_path[PATH_MAX + 1];
	int opt;
	int result;
	char *this_path;

#define DO_REG(_var)							      \
	if (_var) free(_var);						      \
	_var = get_regex(optarg);
#define REG_FREE(_var)							      \
	if (_var) { regfree(_var); free(_var); }

	memset (&args, 0, sizeof(args));
	args.mount_type = mount_to;
	args.netdev = net_ignore;
	args.mounts = rc_stringlist_new();

	while ((opt = getopt_long(argc, argv, getoptstring,
		    longopts, (int *) 0)) != -1)
	{
		switch (opt) {
		case 'e':
			args.netdev = net_yes;
			break;
		case 'E':
			args.netdev = net_no;
			break;
		case 'f':
			DO_REG(args.fstype_regex);
			break;
		case 'F':
			DO_REG(args.skip_fstype_regex);
			break;
		case 'n':
			DO_REG(args.node_regex);
			break;
		case 'N':
			DO_REG(args.skip_node_regex);
			break;
		case 'o':
			DO_REG(args.options_regex);
			break;
		case 'O':
			DO_REG(args.skip_options_regex);
			break;
		case 'p':
			DO_REG(point_regex);
			break;
		case 'P':
			DO_REG(skip_point_regex);
			break;
		case 'i':
			args.mount_type = mount_options;
			break;
		case 's':
			args.mount_type = mount_fstype;
			break;
		case 't':
			args.mount_type = mount_from;
			break;

		case_RC_COMMON_GETOPT
		}
	}

	while (optind < argc) {
		if (argv[optind][0] != '/')
			eerrorx("%s: `%s' is not a mount point",
			    argv[0], argv[optind]);
		this_path = argv[optind++];
		if (realpath(this_path, real_path))
			this_path = real_path;
		rc_stringlist_add(args.mounts, this_path);
	}
	nodes = find_mounts(&args);
	rc_stringlist_free(args.mounts);

	REG_FREE(args.fstype_regex);
	REG_FREE(args.skip_fstype_regex);
	REG_FREE(args.node_regex);
	REG_FREE(args.skip_node_regex);
	REG_FREE(args.options_regex);
	REG_FREE(args.skip_options_regex);

	result = EXIT_FAILURE;

	/* We should report the mounts in reverse order to ease unmounting */
	TAILQ_FOREACH_REVERSE(s, nodes, rc_stringlist, entries) {
		if (point_regex &&
		    regexec(point_regex, s->value, 0, NULL, 0) != 0)
			continue;
		if (skip_point_regex &&
		    regexec(skip_point_regex, s->value, 0, NULL, 0) == 0)
			continue;
		if (! rc_yesno(getenv("EINFO_QUIET")))
			printf("%s\n", s->value);
		result = EXIT_SUCCESS;
	}
	rc_stringlist_free(nodes);

	REG_FREE(point_regex);
	REG_FREE(skip_point_regex);

	return result;
}
