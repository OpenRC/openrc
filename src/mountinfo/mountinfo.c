/*
 * mountinfo.c
 * Obtains information about mounted filesystems.
 */

/*
 * Copyright 2007-2015 The OpenRC Authors.
 * See the Authors file at the top-level directory of this distribution and
 * https://github.com/OpenRC/openrc/blob/HEAD/AUTHORS
 *
 * This file is part of OpenRC. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution and at https://github.com/OpenRC/openrc/blob/HEAD/LICENSE
 * This file may not be copied, modified, propagated, or distributed
 *    except according to the terms contained in the LICENSE file.
 */

#if defined(__DragonFly__) || defined(__FreeBSD__)
#  include <sys/ucred.h>
#  include <sys/mount.h>

#  define F_FLAGS f_flags
#elif defined(__NetBSD__)
#  include <sys/mount.h>

#  define statfs statvfs
#  define F_FLAGS f_flag
#elif defined(BSD) && !defined(__GNU__)
#  include <sys/statvfs.h>

#  define statfs statvfs
#  define F_FLAGS f_flag
#elif defined(__linux__) || (defined(__FreeBSD_kernel__) && \
	defined(__GLIBC__)) || defined(__GNU__)
#  include <mntent.h>
#endif

#include <errno.h>
#include <getopt.h>
#include <regex.h>
#include <stddef.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "einfo.h"
#include "queue.h"
#include "rc.h"
#include "rc_exec.h"
#include "timeutils.h"
#include "_usage.h"
#include "helpers.h"

const char *applet = NULL;
const char *procmounts = "/proc/mounts";
const char *extraopts = "[mount1] [mount2] ...";
const char getoptstring[] = "f:F:n:N:o:O:p:P:iste:E:" getoptstring_COMMON;
const struct option longopts[] = {
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
const char * const longopts_help[] = {
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
const char *usagestring = NULL;

#define UMOUNT_ARGS_MAX 16
#define RUN_MAX 32
#define TRY_MAX 3
#define TRY_DELAY_MS 1000

struct run_queue {
	const char *mntpath;
	int64_t last_exec_time;
	int64_t fuser_exec_time;
	pid_t pid;
	pid_t fuser_pid;
	int try_count;
	int fuser_stdoutfd;
};

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

struct args;
typedef int process_func_t(RC_STRINGLIST *, struct args *,
    char *, char *, char *, char *, int);

struct args {
	process_func_t *process;
	regex_t *node_regex;
	regex_t *skip_node_regex;
	regex_t *fstype_regex;
	regex_t *skip_fstype_regex;
	regex_t *options_regex;
	regex_t *skip_options_regex;
	RC_STRINGLIST *mounts;
	mount_type mount_type;
	net_opts netdev;
	const char *check_mntpath;
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
		if (!s)
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
find_mounts(struct args *args, size_t *num_mounts)
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
				if (!options)
					options = xstrdup(o->o_name);
				else {
					xasprintf(&tmp, "%s,%s", options, o->o_name);
					free(options);
					options = tmp;
				}
			}
			flags &= ~o->o_opt;
		}

		*num_mounts += 0 == args->process(list, args,
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

#elif defined(__linux__) || (defined(__FreeBSD_kernel__) && \
	defined(__GLIBC__)) || defined(__GNU__)
static struct mntent *
getmntfile(const char *file)
{
	struct mntent *ent = NULL;
	FILE *fp;

	if (!(fp = setmntent("/etc/fstab", "r")))
		return NULL;
	while ((ent = getmntent(fp)))
		if (strcmp(file, ent->mnt_dir) == 0)
			break;
	endmntent(fp);

	return ent;
}

static RC_STRINGLIST *
find_mounts(struct args *args, size_t *num_mounts)
{
	FILE *fp;
	char *buffer;
	size_t size;
	char *p;
	char *from;
	char *to;
	char *fst;
	char *opts;
	struct mntent *ent;
	int netdev;
	RC_STRINGLIST *list;

	if ((fp = fopen(procmounts, "r")) == NULL)
		eerrorx("getmntinfo: %s", strerror(errno));

	list = rc_stringlist_new();

	buffer = NULL;
	while (xgetline(&buffer, &size, fp) != -1) {
		netdev = -1;
		p = buffer;
		from = strsep(&p, " ");
		to = strsep(&p, " ");
		fst = strsep(&p, " ");
		opts = strsep(&p, " ");

		if ((ent = getmntfile(to))) {
			if (strstr(ent->mnt_opts, "_netdev"))
				netdev = 0;
			else
				netdev = 1;
		}

		*num_mounts += 0 == args->process(list, args,
			from, to, fst, opts, netdev);

		free(buffer);
		buffer = NULL;
	}
	free(buffer);
	fclose(fp);

	return list;
}

#else
#  error "Operating system not supported!"
#endif

static int is_prefix(const char *needle, const char *hay)
{
	size_t nlen = strlen(needle);
	if (strncmp(needle, hay, nlen) == 0 && hay[nlen] == '/')
		return true;
	return false;
}

static char *unescape_octal(char *beg)
{
	int n, i;
	char *w = beg, *r = beg;
	while (*r) {
		if (*r != '\\' || *++r == '\\') {
			*w++ = *r++;
		} else {
			/* octal. should have at least 3 bytes,
			 * but don't choke on malformed input
			 */
			for (i = n = 0; i < 3; ++i) {
				if (*r >= '0' && *r <= '7') {
					n <<= 3;
					n |= *r++ - '0';
				} else {
					break;
				}
			}
			if (n)
				*w++ = n;
		}
	}
	*w = '\0';
	return beg;
}

static int check_is_mounted(RC_STRINGLIST *list RC_UNUSED, struct args *args,
    char *from RC_UNUSED, char *to, char *fstype RC_UNUSED,
    char *options RC_UNUSED, int netdev RC_UNUSED)
{
	return strcmp(args->check_mntpath, unescape_octal(to)) == 0 ? 0 : -1;
}

static int is_mounted(const char *mntpath)
{
	size_t num_mounts = 0;
	struct args args = { .process = check_is_mounted, .check_mntpath = mntpath };
	RC_STRINGLIST *l = find_mounts(&args, &num_mounts);
	rc_stringlist_free(l);
	return num_mounts > 0;
}

static pid_t run_umount(const char *mntpath,
	const char **umount_args, int umount_args_num)
{
	struct exec_args args;
	struct exec_result res;
	const char *argv[UMOUNT_ARGS_MAX + 3];
	int k, i = 0;

	argv[i++] = "umount";
	for (k = 0; k < umount_args_num; ++k)
		argv[i++] = umount_args[k];
	argv[i++] = mntpath;
	argv[i++] = NULL;

	args = exec_init(argv);
	args.redirect_stdout = args.redirect_stderr = EXEC_DEVNULL;
	res = do_exec(&args);
	if (res.pid < 0)
		eerrorx("%s: failed to run umount: %s", applet, strerror(errno));
	return res.pid;
}

static void fuser_run(struct run_queue *rp, const char *fuser_opt)
{
	static int fuser_exec_failed = 0;
	const char *argv[] = { "fuser", fuser_opt, rp->mntpath, NULL };
	struct exec_result res;
	struct exec_args args;

	/* if exec failed, fuser likely doesn't exist. so don't retry */
	if (fuser_exec_failed)
		return;

	args = exec_init(argv);
	args.redirect_stdout = EXEC_MKPIPE;
	args.redirect_stderr = EXEC_DEVNULL;
	res = do_exec(&args);
	if (res.pid < 0) {
		fuser_exec_failed = 1;
	} else {
		rp->fuser_pid = res.pid;
		rp->fuser_stdoutfd = res.proc_stdout;
		rp->fuser_exec_time = tm_now();
	}
}

static int fuser_decide(struct run_queue *rp,
	const char *fuser_opt, const char *fuser_kill_prefix)
{
	char buf[1<<12];
	char selfpid[64];
	int read_maybe_truncated;
	ssize_t n;

	if (rp->fuser_stdoutfd < 0)
		return 0;

	buf[0] = ' ';
	n = read(rp->fuser_stdoutfd, buf + 1, sizeof buf - 3);
	close(rp->fuser_stdoutfd);
	rp->fuser_stdoutfd = -1;
	read_maybe_truncated = (n == sizeof buf - 3);
	if (n < 0 || read_maybe_truncated)
		return 0;
	while (n > 0 && buf[n] == '\n')
		--n;
	buf[n+1] = ' ';
	buf[n+2] = '\0';
	snprintf(selfpid, sizeof selfpid, " %lld ", (long long)getpid());

	if (strstr(buf, selfpid)) {
		/* lets not kill ourselves */
		eerror("Unmounting %s failed because we are using it", rp->mntpath);
		return -1;
	} else if (strcmp(buf, "  ") == 0) {
		if (rp->try_count >= TRY_MAX) {
			eerror("Unmounting %s failed but fuser finds no one using it", rp->mntpath);
			return -1;
		}
		/* it's possible that whatever was using the mount stopped
		 * using it now, so allow 1 more retry */
		rp->try_count = TRY_MAX;
		return 0;
	} else {
		char sig[32];
		const char *argv[] = {
			"fuser", sig, "-k", fuser_opt, rp->mntpath, NULL
		};
		struct exec_result res;
		struct exec_args args = exec_init(argv);
		args.redirect_stdout = args.redirect_stderr = EXEC_DEVNULL;
		snprintf(sig, sizeof sig, "%s%s", fuser_kill_prefix,
			rp->try_count == TRY_MAX ? "KILL" : "TERM");
		res = do_exec(&args);
		if (res.pid > 0)
			rc_waitpid(res.pid);
		return 0;
	}
}

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

int main(int argc, char **argv)
{
	struct args args;
	regex_t *point_regex = NULL;
	regex_t *skip_point_regex = NULL;
	RC_STRINGLIST *nodes;
	RC_STRING *s;
	char *real_path = NULL;
	int opt;
	int result;
	char *this_path, *argv0;
	const char *tmps;
	const char *fuser_opt, *fuser_kill_prefix;
	size_t num_mounts = 0;
	size_t unmount_index;
	int doing_unmount = 0;
	pid_t pid;
	int status, flags;
	int64_t tmp, next_retry, now;
	int64_t rc_fuser_timeout = -1;
	const char *umount_args[UMOUNT_ARGS_MAX];
	int umount_args_num = 0;
	const char **mounts = NULL;
	struct run_queue running[RUN_MAX] = {0};
	struct run_queue *rp;
	size_t num_running = 0, num_waiting = 0;
	enum { STATE_RUN, STATE_REAP, STATE_RETRY, STATE_END } state;

#define DO_REG(_var)							      \
	if (_var) free(_var);						      \
	_var = get_regex(optarg);
#define REG_FREE(_var)							      \
	if (_var) { regfree(_var); free(_var); }

	argv0 = argv[0];
	applet = basename_c(argv[0]);
	memset (&args, 0, sizeof(args));
	args.mount_type = mount_to;
	args.netdev = net_ignore;
	args.mounts = rc_stringlist_new();
	args.process = process_mount;

	if (strcmp(applet, "do_unmount") == 0) {
		doing_unmount = 1;
		while (argv[1]) {
			/* shift over */
			tmps = argv[1];
			argv[1] = argv0;
			++argv;
			--argc;

			if (strcmp(tmps, "--") == 0)
				break;
			if (umount_args_num >= (int)ARRAY_SIZE(umount_args))
				eerrorx("%s: Too many umount arguments", applet);
			umount_args[umount_args_num++] = tmps;
		}

		tmps = rc_conf_value("rc_fuser_timeout");
		if (tmps && (rc_fuser_timeout = parse_duration(tmps)) < 0)
			ewarn("%s: Invalid rc_fuser_timeout value: `%s`. "
				"Defaulting to 20", applet, tmps);
		if (rc_fuser_timeout < 0)
			rc_fuser_timeout = 20 * 1000;

		tmps = getenv("RC_UNAME");
		if (!tmps || strcmp(tmps, "Linux") == 0) {
			fuser_opt = "-m";
			fuser_kill_prefix = "-";
		} else {
			fuser_opt = "-cm";
			fuser_kill_prefix = "-s";
		}
	}

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
		real_path = realpath(this_path, NULL);
		if (real_path)
			this_path = real_path;
		rc_stringlist_add(args.mounts, this_path);
		free(real_path);
		real_path = NULL;
	}
	nodes = find_mounts(&args, &num_mounts);
	rc_stringlist_free(args.mounts);

	if (doing_unmount)
		mounts = xmalloc(num_mounts * sizeof(*mounts));
	num_mounts = 0;
	result = EXIT_FAILURE;
	/* We should report the mounts in reverse order to ease unmounting */
	TAILQ_FOREACH_REVERSE(s, nodes, rc_stringlist, entries) {
		if (point_regex &&
		    regexec(point_regex, s->value, 0, NULL, 0) != 0)
			continue;
		if (skip_point_regex &&
		    regexec(skip_point_regex, s->value, 0, NULL, 0) == 0)
			continue;
		if (doing_unmount)
			mounts[num_mounts++] = unescape_octal(s->value);
		else if (!rc_yesno(getenv("EINFO_QUIET")))
			printf("%s\n", s->value);
		result = EXIT_SUCCESS;
	}
	if (!doing_unmount)
		goto exit;

	/* STATE_RUN:
	 * can unmount => stays in STATE_RUN
	 * cannot unmount (for any of the reasons below) => STATE_REAP
	 *   (a) nothing left to unmount
	 *   (b) running queue is full
	 *   (c) conflicts with running queue
	 *
	 * STATE_REAP:
	 * successful reap => STATE_RUN
	 * couldn't reap with WNOHANG and there are retries pending => STATE_RETRY
	 * nothing left to reap => STATE_RETRY
	 *
	 * STATE_RETRY:
	 * successfully launched a retry => STATE_REAP
	 * need to wait before retring -> sleep
	 *    sleep successful => STATE_RETRY
	 *    sleep interrupted via SIGCHILD (EINTR) => STATE_REAP
	 * nothing left to retry, reap
	 *    and nothing to run either => STATE_END
	 *    otherwise => STATE_RUN
	 */
	result = EXIT_SUCCESS;
	state = STATE_RUN;
	while (state != STATE_END) switch (state) {
	case STATE_RUN:
		for (unmount_index = 0; unmount_index < num_mounts; ++unmount_index) {
			const char *candidate = mounts[unmount_index];
			int safe_to_unmount = 1;
			for (size_t k = 0; safe_to_unmount && k < num_running; ++k)
				safe_to_unmount = !is_prefix(candidate, running[k].mntpath);
			for (size_t k = 0; safe_to_unmount && k < num_mounts; ++k)
				safe_to_unmount = !is_prefix(candidate, mounts[k]);
			if (!safe_to_unmount)
				continue;
			if (!is_mounted(candidate)) {
				/* probably a shared mount and got unmounted, remove */
				mounts[unmount_index--] = mounts[--num_mounts];
			} else {
				break;
			}
		}
		if (num_running == RUN_MAX || unmount_index >= num_mounts) {
			state = STATE_REAP;
			break;
		}
		rp = running + num_running++;
		rp->mntpath = mounts[unmount_index];
		rp->last_exec_time = tm_now();
		rp->try_count = 0;
		rp->pid = run_umount(rp->mntpath, umount_args, umount_args_num);
		rp->fuser_pid = -1;
		rp->fuser_stdoutfd = -1;
		rp->fuser_exec_time = -1;
		mounts[unmount_index] = mounts[--num_mounts];
		break;
	case STATE_REAP:
		flags = (num_waiting > 0) ? WNOHANG : 0;
		pid = waitpid(-1, &status, flags);
		rp = NULL;
		for (size_t i = 0; i < num_running; ++i) {
			rp = running + i;
			if (rp->fuser_pid == pid)
				rp->fuser_pid = -1;
			if (rp->pid == pid && pid > 0)
				break;
			rp = NULL;
		}
		if (rp) {
			if ((WIFEXITED(status) && WEXITSTATUS(status) == 0) ||
			    !is_mounted(rp->mntpath)) {
				einfo("Unmounted %s", rp->mntpath);
				*rp = running[--num_running];
				state = STATE_RUN;
			} else if (rp->try_count >= TRY_MAX) {
				eerror("Failed to unmount %s", rp->mntpath);
				*rp = running[--num_running];
				result = EXIT_FAILURE;
			} else { /* put into waiting queue */
				rp->pid = -1;
				rp->try_count += 1;
				num_waiting += 1;
				fuser_run(rp, fuser_opt);
			}
		} else {
			state = STATE_RETRY;
		}
		break;
	case STATE_RETRY:
		rp = NULL;
		next_retry = INT64_MAX;
		for (size_t i = 0; i < num_running; ++i) {
			if (running[i].pid > 0)
				continue;
			if (running[i].fuser_pid > 0)
				tmp = running[i].fuser_exec_time + rc_fuser_timeout;
			else
				tmp = running[i].last_exec_time + TRY_DELAY_MS;
			if (tmp < next_retry) {
				rp = running + i;
				next_retry = tmp;
			}
		}
		if (!rp) {
			state = (num_mounts > 0) ? STATE_RUN : STATE_END;
			break;
		}
		now = tm_now();
		if (next_retry > now) {
			int64_t sleep_for = next_retry - now;
			/* a child may become available for reaping *before* we
			 * enter sleep. cap the timeout to stay responsive. */
			if (sleep_for > 500)
				sleep_for = 500;
			if (tm_sleep(sleep_for, 0) != 0 && errno == EINTR)
				state = STATE_REAP;
			now = tm_now();
		}
		if (next_retry <= now) {
			if (rp->fuser_pid > 0) {
				kill(rp->fuser_pid, SIGKILL);
				waitpid(rp->fuser_pid, NULL, 0);
				rp->fuser_pid = -1;
			}
			if (fuser_decide(rp, fuser_opt, fuser_kill_prefix) < 0) { /* abort */
				*rp = running[--num_running];
				result = EXIT_FAILURE;
			} else { /* retry */
				rp->last_exec_time = tm_now();
				rp->pid = run_umount(rp->mntpath,
					umount_args, umount_args_num);
			}
			num_waiting -= 1;
			state = STATE_REAP;
		}
		break;
	default: break;
	}

exit:
	free(mounts);
	rc_stringlist_free(nodes);
	REG_FREE(args.fstype_regex);
	REG_FREE(args.skip_fstype_regex);
	REG_FREE(args.node_regex);
	REG_FREE(args.skip_node_regex);
	REG_FREE(args.options_regex);
	REG_FREE(args.skip_options_regex);
	REG_FREE(point_regex);
	REG_FREE(skip_point_regex);

	return result;
}
