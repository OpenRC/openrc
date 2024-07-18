/*
 * librc
 * core RC functions
 */

/*
 * Copyright (c) 2007-2015 The OpenRC Authors.
 * See the Authors file at the top-level directory of this distribution and
 * https://github.com/OpenRC/openrc/blob/HEAD/AUTHORS
 *
 * This file is part of OpenRC. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution and at https://github.com/OpenRC/openrc/blob/HEAD/LICENSE
 * This file may not be copied, modified, propagated, or distributed
 *    except according to the terms contained in the LICENSE file.
 */

#include <helpers.h>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <limits.h>
#include <regex.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <unistd.h>

#include "queue.h"
#include "librc.h"
#include "misc.h"
#include "rc.h"
#include "einfo.h"
#ifdef __FreeBSD__
#  include <sys/sysctl.h>
#endif

#ifndef S_IXUGO
#  define S_IXUGO (S_IXUSR | S_IXGRP | S_IXOTH)
#endif

/* File stream used for plugins to write environ vars to */
FILE *rc_environ_fd = NULL;

typedef struct rc_service_state_name {
	RC_SERVICE state;
	const char *name;
} rc_service_state_name_t;

/* We MUST list the states below 0x10 first
 * The rest can be in any order */
static const rc_service_state_name_t rc_service_state_names[] = {
	{ RC_SERVICE_STARTED,     "started" },
	{ RC_SERVICE_STOPPED,     "stopped" },
	{ RC_SERVICE_STARTING,    "starting" },
	{ RC_SERVICE_STOPPING,    "stopping" },
	{ RC_SERVICE_INACTIVE,    "inactive" },
	{ RC_SERVICE_WASINACTIVE, "wasinactive" },
	{ RC_SERVICE_HOTPLUGGED,  "hotplugged" },
	{ RC_SERVICE_FAILED,      "failed" },
	{ RC_SERVICE_SCHEDULED,   "scheduled"},
	{ RC_SERVICE_CRASHED,     "crashed"},
	{ 0, NULL}
};

#define LS_INITD	0x01
#define LS_DIR		0x02
static RC_STRINGLIST *
ls_dir(const char *dir, int options)
{
	DIR *dp;
	struct dirent *d;
	RC_STRINGLIST *list = NULL;
	struct stat buf;
	size_t l;
	char *file;
	int r;

	list = rc_stringlist_new();
	if ((dp = opendir(dir)) == NULL)
		return list;
	while (((d = readdir(dp)) != NULL)) {
		if (d->d_name[0] != '.') {
			if (options & LS_INITD) {
				/* Check that our file really exists.
				 * This is important as a service maybe in a
				 * runlevel, but could have been removed. */
				xasprintf(&file, "%s/%s", dir, d->d_name);
				r = stat(file, &buf);
				free(file);
				if (r != 0)
					continue;

				/* .sh files are not init scripts */
				l = strlen(d->d_name);
				if (l > 2 && d->d_name[l - 3] == '.' &&
				    d->d_name[l - 2] == 's' &&
				    d->d_name[l - 1] == 'h')
					continue;
			}
			if (options & LS_DIR) {
				xasprintf(&file, "%s/%s", dir, d->d_name);
				r = stat(file, &buf);
				free(file);
				if (r != 0 || !S_ISDIR(buf.st_mode))
					continue;
			}
			rc_stringlist_add(list, d->d_name);
		}
	}
	closedir(dp);
	return list;
}

static bool
rm_dir(const char *pathname, bool top)
{
	DIR *dp;
	struct dirent *d;
	char *file = NULL;
	struct stat s;
	bool retval = true;

	if ((dp = opendir(pathname)) == NULL)
		return false;

	errno = 0;
	while (((d = readdir(dp)) != NULL) && errno == 0) {
		if (strcmp(d->d_name, ".") != 0 &&
		    strcmp(d->d_name, "..") != 0)
		{
			xasprintf(&file, "%s/%s", pathname, d->d_name);
			if (stat(file, &s) != 0) {
				retval = false;
				break;
			}
			if (S_ISDIR(s.st_mode)) {
				if (!rm_dir(file, true))
				{
					retval = false;
					break;
				}
			} else {
				if (unlink(file)) {
					retval = false;
					break;
				}
			}
			free(file);
			file = NULL;
		}
	}
	free(file);
	closedir(dp);

	if (!retval)
		return false;

	if (top && rmdir(pathname) != 0)
		return false;

	return true;
}

/* Other systems may need this at some point, but for now it's Linux only */
#ifdef __linux__
static bool
file_regex(const char *file, const char *regex)
{
	FILE *fp;
	char *line = NULL;
	size_t len = 0;
	regex_t re;
	bool retval = true;
	int result;

	if (!(fp = fopen(file, "r")))
		return false;

	if ((result = regcomp(&re, regex, REG_EXTENDED | REG_NOSUB)) != 0) {
		fclose(fp);
		line = xmalloc(sizeof(char) * BUFSIZ);
		regerror(result, &re, line, BUFSIZ);
		fprintf(stderr, "file_regex: %s", line);
		free(line);
		return false;
	}

	while ((rc_getline(&line, &len, fp))) {
		char *str = line;
		/* some /proc files have \0 separated content so we have to
		   loop through the 'line' */
		do {
			if (regexec(&re, str, 0, NULL, 0) == 0)
				goto found;
			str += strlen(str) + 1;
			/* len is the size of allocated buffer and we don't
			   want call regexec BUFSIZE times. find next str */
			while (str < line + len && *str == '\0')
				str++;
		} while (str < line + len);
	}
	retval = false;
found:
	fclose(fp);
	free(line);
	regfree(&re);

	return retval;
}
#endif


static const char *
get_systype(void)
{
	char *systype = rc_conf_value("rc_sys");
	if (systype) {
		char *s = systype;
		/* Convert to uppercase */
		while (s && *s) {
			if (islower((unsigned char) *s))
				*s = toupper((unsigned char) *s);
			s++;
		}
	}
	return systype;
}

static const char *
detect_prefix(const char *systype)
{
#ifdef PREFIX
	return RC_SYS_PREFIX;
#else
	if (systype) {
		if (strcmp(systype, RC_SYS_NONE) == 0)
			return NULL;
		if (strcmp(systype, RC_SYS_PREFIX) == 0)
			return RC_SYS_PREFIX;
	}

	return NULL;
#endif
}

static const char *
detect_container(const char *systype RC_UNUSED)
{
#ifdef __FreeBSD__
	if (systype) {
		if (strcmp(systype, RC_SYS_NONE) == 0)
		       return NULL;
		if (strcmp(systype, RC_SYS_JAIL) == 0)
			return RC_SYS_JAIL;
	}

	int jailed = 0;
	size_t len = sizeof(jailed);

	if (sysctlbyname("security.jail.jailed", &jailed, &len, NULL, 0) == 0)
		if (jailed == 1)
			return RC_SYS_JAIL;
#endif

#ifdef __linux__
	if (systype) {
		if (strcmp(systype, RC_SYS_NONE) == 0)
			return NULL;
		if (strcmp(systype, RC_SYS_UML) == 0)
			return RC_SYS_UML;
		if (strcmp(systype, RC_SYS_VSERVER) == 0)
			return RC_SYS_VSERVER;
		if (strcmp(systype, RC_SYS_OPENVZ) == 0)
			return RC_SYS_OPENVZ;
		if (strcmp(systype, RC_SYS_LXC) == 0)
			return RC_SYS_LXC;
		if (strcmp(systype, RC_SYS_RKT) == 0)
				return RC_SYS_RKT;
		if (strcmp(systype, RC_SYS_SYSTEMD_NSPAWN) == 0)
				return RC_SYS_SYSTEMD_NSPAWN;
		if (strcmp(systype, RC_SYS_DOCKER) == 0)
				return RC_SYS_DOCKER;
	}
	if (file_regex("/proc/cpuinfo", "UML"))
		return RC_SYS_UML;
	else if (file_regex("/proc/self/status",
		"(s_context|VxID):[[:space:]]*[1-9]"))
		return RC_SYS_VSERVER;
	else if (exists("/proc/vz/veinfo") && !exists("/proc/vz/version"))
		return RC_SYS_OPENVZ;
	else if (file_regex("/proc/self/status",
		"envID:[[:space:]]*[1-9]"))
		return RC_SYS_OPENVZ; /* old test */
	else if (file_regex("/proc/1/environ", "container=lxc"))
		return RC_SYS_LXC;
	else if (file_regex("/proc/1/environ", "container=rkt"))
		return RC_SYS_RKT;
	else if (file_regex("/proc/1/environ", "container=systemd-nspawn"))
		return RC_SYS_SYSTEMD_NSPAWN;
	else if (exists("/.dockerenv"))
		return RC_SYS_DOCKER;
	/* old test, I'm not sure when this was valid. */
	else if (file_regex("/proc/1/environ", "container=docker"))
		return RC_SYS_DOCKER;
#endif

	return NULL;
}

static const char *
detect_vm(const char *systype RC_UNUSED)
{
#ifdef __NetBSD__
	if (systype) {
		if (strcmp(systype, RC_SYS_NONE) == 0)
			return NULL;
		if (strcmp(systype, RC_SYS_XEN0) == 0)
			return RC_SYS_XEN0;
		if (strcmp(systype, RC_SYS_XENU) == 0)
			return RC_SYS_XENU;
	}
	if (exists("/kern/xen/privcmd"))
		return RC_SYS_XEN0;
	if (exists("/kern/xen"))
		return RC_SYS_XENU;
#endif

#ifdef __linux__
	if (systype) {
		if (strcmp(systype, RC_SYS_NONE) == 0)
			return NULL;
		if (strcmp(systype, RC_SYS_XEN0) == 0)
			return RC_SYS_XEN0;
		if (strcmp(systype, RC_SYS_XENU) == 0)
			return RC_SYS_XENU;
	}
	if (exists("/proc/xen")) {
		if (file_regex("/proc/xen/capabilities", "control_d"))
			return RC_SYS_XEN0;
		return RC_SYS_XENU;
	}
#endif

	return NULL;
}

const char *
rc_sys(void)
{
	const char *systype;
	const char *sys;

	systype = get_systype();
	sys = detect_prefix(systype);
	if (!sys) {
		sys = detect_container(systype);
		if (!sys) {
			sys = detect_vm(systype);
		}
	}

	return sys;
}

static const char *
rc_parse_service_state(RC_SERVICE state)
{
	int i;

	for (i = 0; rc_service_state_names[i].name; i++) {
		if (rc_service_state_names[i].state == state)
			return rc_service_state_names[i].name;
	}
	return NULL;
}

/* Returns a list of all the chained runlevels used by the
 * specified runlevel in dependency order, including the
 * specified runlevel. */
static void
get_runlevel_chain(const char *runlevel, RC_STRINGLIST *level_list, RC_STRINGLIST *ancestor_list)
{
	char *path;
	RC_STRINGLIST *dirs;
	RC_STRING *d, *parent;
	const char *nextlevel;

	/*
	 * If we haven't been passed a runlevel or a level list, or
	 * if the passed runlevel doesn't exist then we're done already!
	 */
	if (!runlevel || !level_list || !rc_runlevel_exists(runlevel))
		return;

	/*
	 * We want to add this runlevel to the list but if
	 * it is already in the list it needs to go at the
	 * end again.
	 */
	if (rc_stringlist_find(level_list, runlevel))
		rc_stringlist_delete(level_list, runlevel);
	rc_stringlist_add(level_list, runlevel);

	/*
	 * We can now do exactly the above procedure for our chained
	 * runlevels.
	 */
	xasprintf(&path, "%s/%s", rc_runlevel_dir(), runlevel);
	dirs = ls_dir(path, LS_DIR);
	TAILQ_FOREACH(d, dirs, entries) {
		nextlevel = d->value;

		/* Check for loop */
		if (rc_stringlist_find(ancestor_list, nextlevel)) {
			fprintf(stderr, "Loop detected in stacked runlevels attempting to enter runlevel %s!\n",
			    nextlevel);
			fprintf(stderr, "Ancestors:\n");
			TAILQ_FOREACH(parent, ancestor_list, entries)
				fprintf(stderr, "\t%s\n", parent->value);
			exit(1);
		}

		/* Add new ancestor */
		rc_stringlist_add(ancestor_list, nextlevel);

		get_runlevel_chain(nextlevel, level_list, ancestor_list);

		rc_stringlist_delete(ancestor_list, nextlevel);
	}
	rc_stringlist_free(dirs);
	free(path);
}

bool
rc_runlevel_starting(void)
{
	char *stopping_dir;
	bool found;

	xasprintf(&stopping_dir, "%s/%s", rc_service_dir(), RC_STARTING);
	found = exists(stopping_dir);
	free(stopping_dir);

	return found;
}

bool
rc_runlevel_stopping(void)
{
	char *stopping_dir;
	bool found;

	xasprintf(&stopping_dir, "%s/%s", rc_service_dir(), RC_STOPPING);
	found = exists(stopping_dir);
	free(stopping_dir);

	return found;
}

RC_STRINGLIST *rc_runlevel_list(void)
{
	return ls_dir(rc_runlevel_dir(), LS_DIR);
}

char *
rc_runlevel_get(void)
{
	FILE *fp;
	char *softlevel;
	char *runlevel = NULL;
	size_t i;

	xasprintf(&softlevel, "%s/softlevel", rc_service_dir());
	if ((fp = fopen(softlevel, "r"))) {
		runlevel = xmalloc(sizeof(char) * PATH_MAX);
		if (fgets(runlevel, PATH_MAX, fp)) {
			i = strlen(runlevel) - 1;
			if (runlevel[i] == '\n')
				runlevel[i] = 0;
		} else
			*runlevel = '\0';
		fclose(fp);
	}
	free(softlevel);

	if (!runlevel || !*runlevel) {
		free(runlevel);
		runlevel = xstrdup(RC_LEVEL_SYSINIT);
	}

	return runlevel;
}

bool
rc_runlevel_set(const char *runlevel)
{
	char *softlevel;
	FILE *fp;

	xasprintf(&softlevel, "%s/softlevel", rc_service_dir());
	fp = fopen(softlevel, "w");
	if (!fp)
		return false;
	fprintf(fp, "%s", runlevel);
	fclose(fp);
	free(softlevel);
	return true;
}

bool
rc_runlevel_exists(const char *runlevel)
{
	char *path;
	struct stat buf;
	int r;

	if (!runlevel || strcmp(runlevel, "") == 0 || strcmp(runlevel, ".") == 0 ||
		strcmp(runlevel, "..") == 0)
		return false;
	xasprintf(&path, "%s/%s", rc_runlevel_dir(), runlevel);
	r = stat(path, &buf);
	free(path);
	if (r == 0 && S_ISDIR(buf.st_mode))
		return true;
	return false;
}

bool
rc_runlevel_stack(const char *dst, const char *src)
{
	char *d, *s;
	int r;

	if (!rc_runlevel_exists(dst) || !rc_runlevel_exists(src))
		return false;
	xasprintf(&s, "../%s", src);
	xasprintf(&d, "%s/%s/%s", rc_runlevel_dir(), dst, src);
	r = symlink(s, d);
	free(d);
	free(s);
	return (r == 0);
}

bool
rc_runlevel_unstack(const char *dst, const char *src)
{
	char *path;
	int r;

	xasprintf(&path, "%s/%s/%s", rc_runlevel_dir(), dst, src);
	r = unlink(path);
	free(path);
	return (r == 0);
}

RC_STRINGLIST *
rc_runlevel_stacks(const char *runlevel)
{
	RC_STRINGLIST *stack, *ancestor_list;
	stack = rc_stringlist_new();
	ancestor_list = rc_stringlist_new();
	rc_stringlist_add(ancestor_list, runlevel);
	get_runlevel_chain(runlevel, stack, ancestor_list);
	rc_stringlist_free(ancestor_list);
	return stack;
}

static struct {
	char *userconf;
	char *runlevels;
	char *service;
} rc_user_dirs;

static void
free_rc_user_dirs(void)
{
	free(rc_user_dirs.userconf);
	rc_user_dirs.userconf = NULL;
	free(rc_user_dirs.runlevels);
	rc_user_dirs.runlevels = NULL;
	free(rc_user_dirs.service);
	rc_user_dirs.service = NULL;
}

static void
init_rc_user_dirs(void)
{
	char *env;

	if ((env = getenv("XDG_CONFIG_HOME")))
		xasprintf(&rc_user_dirs.userconf, "%s/openrc", env);
	else if ((env = getenv("HOME")))
		xasprintf(&rc_user_dirs.userconf, "%s/.config/openrc", env);
	else
		eerrorx("XDG_CONFIG_HOME and HOME unset");

	xasprintf(&rc_user_dirs.runlevels, "%s/%s", rc_user_dirs.userconf, RC_RUNLEVEL_SUBDIR);

	if (!(env = getenv("XDG_RUNTIME_DIR")))
		eerrorx("XDG_RUNTIME_DIR unset."); /* FIXME: fallback to something else? */
	xasprintf(&rc_user_dirs.service, "%s/openrc", env);
	atexit(free_rc_user_dirs);
}

static bool is_user = false;

void
rc_set_user(void)
{
	if (!is_user)
		init_rc_user_dirs();
	is_user = true;
	setenv("RC_USER_SERVICES", "yes", true);
}

bool
rc_is_user(void)
{
	return is_user;
}

const char *
rc_sysconf_dir(void)
{
	if (rc_is_user())
		return RC_SYSCONFDIR "/" RC_USER_SUBDIR;
	else
		return RC_SYSCONFDIR;
}

const char *
rc_userconf_dir(void)
{
	if (!rc_is_user())
		return NULL;

	if (!rc_user_dirs.userconf)
		eerrorx("rc_userconf_dir called in user mode without paths set");

	return rc_user_dirs.userconf;
}

const char *
rc_runlevel_dir(void)
{
	if (!rc_is_user())
		return RC_RUNLEVELDIR;

	if (!rc_user_dirs.runlevels)
		eerrorx("rc_runlevel_dir called in user mode without paths set");

	return rc_user_dirs.runlevels;
}

const char *
rc_service_dir(void)
{
	if (!rc_is_user())
		return RC_SVCDIR;

	if (!rc_user_dirs.service)
		eerrorx("rc_service_dir called in user mode without paths set");

	return rc_user_dirs.service;
}

static ssize_t
safe_readlink(const char *path, char **buf, size_t bufsiz)
{
	char *buffer;
	ssize_t r;

	for (;; bufsiz += PATH_MAX) {
		buffer = xmalloc(bufsiz);
		r = readlink(path, buffer, bufsiz);
		if (r < 0) {
			free(buffer);
			return -errno;
		}
		if ((size_t)r < bufsiz) {
			buffer[r] = '\0';
			*buf = buffer;
			return r;
		}
		free(buffer);
		if (bufsiz >= SSIZE_MAX - PATH_MAX)
			return -ENOMEM;
	}
}

/* Resolve a service name to its full path */
char *
rc_service_resolve(const char *service)
{
	char *buffer;
	char *file = NULL;
	struct stat buf;
	const char *sysconf_dir = rc_sysconf_dir();

	if (!service)
		return NULL;

	if (service[0] == '/')
		return xstrdup(service);

	/* First check started services */
	xasprintf(&file, "%s/%s/%s", rc_service_dir(), "started", service);
	if (lstat(file, &buf) || !S_ISLNK(buf.st_mode)) {
		free(file);
		xasprintf(&file, "%s/%s/%s", rc_service_dir(), "inactive", service);
		if (lstat(file, &buf) || !S_ISLNK(buf.st_mode)) {
			free(file);
			file = NULL;
		}
	}

	if (file != NULL && safe_readlink(file, &buffer, buf.st_size + 1) >= 0) {
		free(file);
		return buffer;
	}

	/* Check user specific scripts */
	if (rc_is_user()) {
		/* Local user config takes priority */
		xasprintf(&file, "%s/%s/%s", rc_userconf_dir(), RC_INIT_SUBDIR, service);
		if (stat(file, &buf) == 0)
			return file;

		free(file);
		xasprintf(&file, "%s/%s/%s", sysconf_dir, RC_INIT_SUBDIR, service);
		if (stat(file, &buf) == 0)
			return file;

		free(file);
		return NULL;
	}

#ifdef RC_LOCAL_INITDIR
	/* Nope, so lets see if the user has written it */
	free(file);
	xasprintf(&file, "%s/%s", RC_LOCAL_INITDIR, service);
	if (stat(file, &buf) == 0)
		return file;
#endif

	/* System scripts take precedence over 3rd party ones */
	free(file);
	xasprintf(&file, "%s/%s/%s", sysconf_dir, RC_INIT_SUBDIR, service);
	if (stat(file, &buf) == 0)
		return file;

#ifdef RC_PKG_INITDIR
	/* Check RC_PKG_INITDIR */
	free(file);
	xasprintf(&file, "%s/%s", RC_PKG_INITDIR, service);
	if (stat(file, &buf) == 0)
		return file;
#endif

	free(file);
	return NULL;
}

bool
rc_service_exists(const char *service)
{
	char *file;
	bool retval = false;
	size_t len;
	struct stat buf;

	if (!service) {
		errno = EINVAL;
		return false;
	}

	len = strlen(service);

	/* .sh files are not init scripts */
	if (len > 2 && service[len - 3] == '.' &&
	    service[len - 2] == 's' &&
	    service[len - 1] == 'h') {
		errno = EINVAL;
		return false;
	}

	if (!(file = rc_service_resolve(service))) {
		errno = ENOENT;
		return false;
	}

	if (stat(file, &buf) == 0) {
		if (buf.st_mode & S_IXUGO)
			retval = true;
		else
			errno = ENOEXEC;
	}
	free(file);
	return retval;
}

#define OPTSTR \
". '%s'; echo $extra_commands $extra_started_commands $extra_stopped_commands"

RC_STRINGLIST *
rc_service_extra_commands(const char *service)
{
	char *svc;
	char *cmd = NULL;
	char *buffer = NULL;
	size_t len = 0;
	RC_STRINGLIST *commands = NULL;
	char *token;
	char *p;
	FILE *fp;
	size_t l;

	if (!(svc = rc_service_resolve(service)))
		return NULL;

	l = strlen(OPTSTR) + strlen(svc) + 1;
	cmd = xmalloc(sizeof(char) * l);
	snprintf(cmd, l, OPTSTR, svc);
	free(svc);

	if ((fp = popen(cmd, "r"))) {
		rc_getline(&buffer, &len, fp);
		p = buffer;
		commands = rc_stringlist_new();

		while ((token = strsep(&p, " ")))
			if (token[0] != '\0')
				rc_stringlist_add(commands, token);

		pclose(fp);
		free(buffer);
	}

	free(cmd);
	return commands;
}

#define DESCSTR ". '%s'; echo \"${description%s%s}\""
char *
rc_service_description(const char *service, const char *option)
{
	char *svc;
	char *cmd;
	char *desc = NULL;
	size_t len = 0;
	FILE *fp;
	size_t l;

	if (!(svc = rc_service_resolve(service)))
		return NULL;

	if (!option)
		option = "";

	l = strlen(DESCSTR) + strlen(svc) + strlen(option) + 2;
	cmd = xmalloc(sizeof(char) * l);
	snprintf(cmd, l, DESCSTR, svc, *option ? "_" : "", option);
	free(svc);
	if ((fp = popen(cmd, "r"))) {
		rc_getline(&desc, &len, fp);
		pclose(fp);
	}
	free(cmd);
	return desc;
}

bool
rc_service_in_runlevel(const char *service, const char *runlevel)
{
	char *file;
	bool r;

	xasprintf(&file, "%s/%s/%s", rc_runlevel_dir(), runlevel, basename_c(service));
	r = exists(file);
	free(file);
	return r;
}

bool
rc_service_mark(const char *service, const RC_SERVICE state)
{
	char *file, *was;
	int i = 0;
	int skip_state = -1;
	const char *base;
	char *init = rc_service_resolve(service);
	const char *svc_dir = rc_service_dir();
	bool skip_wasinactive = false;
	int s;
	RC_STRINGLIST *dirs;
	RC_STRING *dir;
	int serrno;

	if (!init)
		return false;

	base = basename_c(service);
	if (state != RC_SERVICE_STOPPED) {
		if (!exists(init)) {
			free(init);
			return false;
		}

		xasprintf(&file, "%s/%s/%s", svc_dir,
		    rc_parse_service_state(state), base);
		if (exists(file))
			unlink(file);
		i = symlink(init, file);
		free(file);
		if (i != 0) {
			free(init);
			return false;
		}
		skip_state = state;
	}

	if (state == RC_SERVICE_HOTPLUGGED || state == RC_SERVICE_FAILED) {
		free(init);
		return true;
	}

	/* Remove any old states now */
	for (i = 0; rc_service_state_names[i].name; i++) {
		s = rc_service_state_names[i].state;

		if ((s != skip_state &&
			s != RC_SERVICE_STOPPED &&
			s != RC_SERVICE_HOTPLUGGED &&
			s != RC_SERVICE_SCHEDULED) &&
		    (!skip_wasinactive || s != RC_SERVICE_WASINACTIVE))
		{
			xasprintf(&file, "%s/%s/%s", rc_service_dir(),
			    rc_service_state_names[i].name, base);
			if (exists(file)) {
				if ((state == RC_SERVICE_STARTING ||
					state == RC_SERVICE_STOPPING) &&
				    s == RC_SERVICE_INACTIVE)
				{
					xasprintf(&was, "%s/%s/%s", rc_service_dir(),
					    rc_parse_service_state(RC_SERVICE_WASINACTIVE),
					    base);
					i = symlink(init, was);
					free(was);
					if (i == -1) {
						free(file);
						free(init);
						return false;
					}
					skip_wasinactive = true;
				}
				if (unlink(file) == -1) {
					free(file);
					free(init);
					return false;
				}
			}
			free(file);
		}
	}

	/* Remove the exclusive state if we're inactive */
	if (state == RC_SERVICE_STARTED ||
	    state == RC_SERVICE_STOPPED ||
	    state == RC_SERVICE_INACTIVE)
	{
		xasprintf(&file, "%s/%s/%s", svc_dir, "exclusive", base);
		unlink(file);
		free(file);
	}

	/* Remove any options and daemons the service may have stored */
	if (state == RC_SERVICE_STOPPED) {
		xasprintf(&file, "%s/%s/%s", svc_dir, "options", base);
		rm_dir(file, true);
		free(file);

		xasprintf(&file, "%s/%s/%s", svc_dir, "daemons", base);
		rm_dir(file, true);
		free(file);

		rc_service_schedule_clear(service);
	}

	/* These are final states, so remove us from scheduled */
	if (state == RC_SERVICE_STARTED || state == RC_SERVICE_STOPPED) {
		xasprintf(&file, "%s/%s", svc_dir, "scheduled");
		dirs = ls_dir(file, 0);
		TAILQ_FOREACH(dir, dirs, entries) {
			xasprintf(&was, "%s/%s/%s", file, dir->value, base);
			unlink(was);
			free(was);

			/* Try and remove the dir; we don't care about errors */
			xasprintf(&was, "%s/%s", file, dir->value);
			serrno = errno;
			rmdir(was);
			errno = serrno;
			free(was);
		}
		free(file);
		rc_stringlist_free(dirs);
	}
	free(init);
	return true;
}

RC_SERVICE
rc_service_state(const char *service)
{
	int i;
	int state = RC_SERVICE_STOPPED;
	char *file;
	RC_STRINGLIST *dirs;
	RC_STRING *dir;
	const char *base = basename_c(service);
	const char *svc_dir = rc_service_dir();

	for (i = 0; rc_service_state_names[i].name; i++) {
		xasprintf(&file, "%s/%s/%s", svc_dir, rc_service_state_names[i].name, base);
		if (exists(file)) {
			if (rc_service_state_names[i].state <= 0x10)
				state = rc_service_state_names[i].state;
			else
				state |= rc_service_state_names[i].state;
		}
		free(file);
	}

	if (state & RC_SERVICE_STARTED) {
		if (rc_service_daemons_crashed(service) && errno != EACCES)
			state |= RC_SERVICE_CRASHED;
	}
	if (state & RC_SERVICE_STOPPED) {
		char *path;
		xasprintf(&path, "%s/scheduled", svc_dir);
		dirs = ls_dir(path, 0);
		free(path);
		TAILQ_FOREACH(dir, dirs, entries) {
			xasprintf(&file, "%s/scheduled/%s/%s", svc_dir, dir->value, service);
			i = exists(file);
			free(file);
			if (i) {
				state |= RC_SERVICE_SCHEDULED;
				break;
			}
		}
		rc_stringlist_free(dirs);
	}

	return state;
}

char *
rc_service_value_get(const char *service, const char *option)
{
	char *buffer = NULL;
	size_t len = 0;
	char *file;

	xasprintf(&file, "%s/options/%s/%s", rc_service_dir(), service, option);
	rc_getfile(file, &buffer, &len);
	free(file);

	return buffer;
}

bool
rc_service_value_set(const char *service, const char *option,
    const char *value)
{
	FILE *fp;
	char *file1, *file2;

	xasprintf(&file1, "%s/options/%s", rc_service_dir(), service);
	if (mkdir(file1, 0755) != 0 && errno != EEXIST) {
		free(file1);
		return false;
	}

	xasprintf(&file2, "%s/%s", file1, option);
	free(file1);
	if (value) {
		fp = fopen(file2, "w");
		free(file2);
		if (fp == NULL)
			return false;
		fprintf(fp, "%s", value);
		fclose(fp);
	} else {
		unlink(file2);
		free(file2);
	}
	return true;
}

bool
rc_service_schedule_start(const char *service, const char *service_to_start)
{
	char *file1, *file2;
	char *init;
	bool retval;

	/* service may be a provided service, like net */
	if (!service || !rc_service_exists(service_to_start))
		return false;

	xasprintf(&file1, "%s/scheduled/%s", rc_service_dir(), basename_c(service));
	if (mkdir(file1, 0755) != 0 && errno != EEXIST) {
		free(file1);
		return false;
	}

	init = rc_service_resolve(service_to_start);
	xasprintf(&file2, "%s/%s", file1, basename_c(service_to_start));
	free(file1);
	retval = (exists(file2) || symlink(init, file2) == 0);
	free(file2);
	free(init);
	return retval;
}

bool
rc_service_schedule_clear(const char *service)
{
	char *dir;
	bool r;

	xasprintf(&dir, "%s/scheduled/%s", rc_service_dir(), basename_c(service));
	r = rm_dir(dir, true);
	free(dir);
	if (!r && errno == ENOENT)
		return true;
	return false;
}

RC_STRINGLIST *
rc_services_in_runlevel(const char *runlevel)
{
	char *dir;
	RC_STRINGLIST *list = NULL;
	const char *sysconf_dir = rc_sysconf_dir();

	if (!runlevel) {
		const char *userconf_dir;
		char *init_dir;

		xasprintf(&init_dir, "%s/%s", sysconf_dir, RC_INIT_SUBDIR);
		list = ls_dir(init_dir, LS_INITD);
		free(init_dir);

		if ((userconf_dir = rc_userconf_dir())) {
			RC_STRINGLIST *usr = NULL;
			xasprintf(&init_dir, "%s/%s", userconf_dir, RC_INIT_SUBDIR);

			usr = ls_dir(init_dir, LS_INITD);
			TAILQ_CONCAT(list, usr, entries);
			rc_stringlist_free(usr);

			free(init_dir);
			return list;
		}

#ifdef RC_PKG_INITDIR
		{
			RC_STRINGLIST *pkg = ls_dir(RC_PKG_INITDIR, LS_INITD);
			TAILQ_CONCAT(list, pkg, entries);
			rc_stringlist_free(pkg);
		}
#endif
#ifdef RC_LOCAL_INITDIR
		{
			RC_STRINGLIST *local = ls_dir(RC_LOCAL_INITDIR, LS_INITD);
			TAILQ_CONCAT(list, local, entries);
			rc_stringlist_free(local);
		}
#endif
		return list;
	}

	/* These special levels never contain any services */
	if (strcmp(runlevel, RC_LEVEL_SINGLE) != 0) {
		xasprintf(&dir, "%s/%s", rc_runlevel_dir(), runlevel);
		list = ls_dir(dir, LS_INITD);
		free(dir);
	}

	if (!list)
		list = rc_stringlist_new();
	return list;
}

RC_STRINGLIST *
rc_services_in_runlevel_stacked(const char *runlevel)
{
	RC_STRINGLIST *list, *stacks, *sl;
	RC_STRING *stack;

	list = rc_services_in_runlevel(runlevel);
	stacks = rc_runlevel_stacks(runlevel);
	TAILQ_FOREACH(stack, stacks, entries) {
		sl = rc_services_in_runlevel(stack->value);
		TAILQ_CONCAT(list, sl, entries);
		rc_stringlist_free(sl);
	}
	rc_stringlist_free(stacks);
	return list;
}

RC_STRINGLIST *
rc_services_in_state(RC_SERVICE state)
{
	RC_STRINGLIST *services;
	RC_STRINGLIST *list;
	RC_STRINGLIST *dirs;
	RC_STRING *d;
	char *dir, *dir2;

	xasprintf(&dir, "%s/%s", rc_service_dir(), rc_parse_service_state(state));

	if (state != RC_SERVICE_SCHEDULED) {
		dirs = ls_dir(dir, LS_INITD);
		free(dir);
		return dirs;
	}

	dirs = ls_dir(dir, 0);
	list = rc_stringlist_new();
	if (!dirs) {
		free(dir);
		return list;
	}

	TAILQ_FOREACH(d, dirs, entries) {
		xasprintf(&dir2, "%s/%s", dir, d->value);
		services = ls_dir(dir2, LS_INITD);
		free(dir2);
		if (services) {
			TAILQ_CONCAT(list, services, entries);
			rc_stringlist_free(services);
		}
	}
	rc_stringlist_free(dirs);
	free(dir);
	return list;
}

bool
rc_service_add(const char *runlevel, const char *service)
{
	bool retval;
	char *init;
	char *file;
	char *path;
	char *binit = NULL;
	char *i;

	if (!rc_runlevel_exists(runlevel)) {
		errno = ENOENT;
		return false;
	}

	if (rc_service_in_runlevel(service, runlevel)) {
		errno = EEXIST;
		return false;
	}

	i = init = rc_service_resolve(service);

	/* We need to ensure that only things in /etc/init.d are added
	 * to the boot runlevel */
	if (!rc_is_user() && strcmp(runlevel, RC_LEVEL_BOOT) == 0) {
		path = realpath(dirname(init), NULL);
		if (path == NULL) {
			free(init);
			return false;
		}
		retval = (strcmp(path, RC_INITDIR) == 0);
		free(path);
		if (!retval) {
			free(init);
			errno = EPERM;
			return false;
		}
		xasprintf(&binit, "%s/%s/%s", rc_service_dir(), RC_INIT_SUBDIR, service);
		i = binit;
	}

	xasprintf(&file, "%s/%s/%s", RC_RUNLEVELDIR, runlevel,
		basename_c(service));
	retval = (symlink(i, file) == 0);
	free(file);
	free(binit);
	free(init);
	return retval;
}

bool
rc_service_delete(const char *runlevel, const char *service)
{
	char *file;
	int r;

	xasprintf(&file, "%s/%s/%s", rc_runlevel_dir(), runlevel, basename_c(service));
	r = unlink(file);
	free(file);
	return (r == 0);
}

char *
rc_service_base(const char *service)
{
	char *dot = strchr(service, '.');
	if (!dot)
		return NULL;
	return xstrndup(service, dot - service);
}

char *
rc_service_dylink(const char *service, const char *target)
{
	char *file = rc_service_resolve(service);
	char *target_file = NULL;
	if (!exists(file))
		goto out;

	xasprintf(&target_file, "%s/dynamic/%s", rc_service_dir(), target);
	if (!exists(target_file) && symlink(file, target_file) == -1) {
		free(target_file);
		target_file = NULL;
		goto out;
	}

out:
	free(file);
	return target_file;
}

RC_STRINGLIST *
rc_services_scheduled_by(const char *service)
{
	RC_STRINGLIST *dirs;
	RC_STRINGLIST *list = rc_stringlist_new();
	RC_STRING *dir;
	char *file, *scheduled_dir;

	xasprintf(&scheduled_dir, "%s/scheduled", rc_service_dir());
	dirs = ls_dir(scheduled_dir, 0);
	free(scheduled_dir);

	TAILQ_FOREACH(dir, dirs, entries) {
		xasprintf(&file, "%s/scheduled/%s/%s", rc_service_dir(), dir->value, service);
		if (exists(file))
			rc_stringlist_add(list, file);
		free(file);
	}
	rc_stringlist_free(dirs);
	return list;
}

RC_STRINGLIST *
rc_services_scheduled(const char *service)
{
	char *dir;
	RC_STRINGLIST *list;

	xasprintf(&dir, "%s/scheduled/%s", rc_service_dir(), basename_c(service));
	list = ls_dir(dir, LS_INITD);
	free(dir);
	return list;
}
