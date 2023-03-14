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
#include "einfo.h"
#include "misc.h"
#include "rc.h"
#ifdef __FreeBSD__
#  include <sys/sysctl.h>
#endif

#define RC_RUNLEVEL_FOLDER "/softlevel"
#define RC_RUNLEVEL RC_SVCDIR RC_RUNLEVEL_FOLDER

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
	char file[PATH_MAX];
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
				snprintf(file, sizeof(file), "%s/%s",
				    dir, d->d_name);
				r = stat(file, &buf);
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
				snprintf(file, sizeof(file), "%s/%s",
				    dir, d->d_name);
				if (stat(file, &buf) != 0 ||
				    !S_ISDIR(buf.st_mode))
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
	char file[PATH_MAX];
	struct stat s;
	bool retval = true;

	if ((dp = opendir(pathname)) == NULL)
		return false;

	errno = 0;
	while (((d = readdir(dp)) != NULL) && errno == 0) {
		if (strcmp(d->d_name, ".") != 0 &&
		    strcmp(d->d_name, "..") != 0)
		{
			snprintf(file, sizeof(file),
			    "%s/%s", pathname, d->d_name);
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
		}
	}
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

#ifdef RC_USER_SERVICES

bool
rc_is_user(void)
{
	char *env;
	if ((env = getenv("RC_USER_SERVICES"))) {
		return (strcmp(env, "YES") == 0);
	}
	return false;
}

void
rc_set_user(void)
{
	char *path, *tmp;

	/* Setting the sysconf path to XDG_CONFIG_HOME, or ~/.config/, so subdirectories would go:
	* ~/.config/init.d
	* ~/.config/conf.d
	* ~/.config/runlevels
	* ~/.config/rc.conf */
	path = rc_user_sysconfdir();
	if (mkdir(path, 0700) != 0 && errno != EEXIST) {
		eerrorx("mkdir: %s", strerror(errno));
	}
	xasprintf(&tmp, "%s/%s", path, RC_USER_INITDIR_FOLDER);
	if (mkdir(tmp, 0700) != 0 && errno != EEXIST) {
		eerrorx("mkdir: %s", strerror(errno));
	}
	free(tmp);
	xasprintf(&tmp, "%s/%s", path, RC_USER_CONFDIR_FOLDER);
	if (mkdir(tmp, 0700) != 0 && errno != EEXIST) {
		eerrorx("mkdir: %s", strerror(errno));
	}
	free(tmp);
	xasprintf(&tmp, "%s/%s", path, RC_USER_RUNLEVELS_FOLDER);
	if (mkdir(tmp, 0700) != 0 && errno != EEXIST) {
		eerrorx("mkdir: %s", strerror(errno));
	}
	free(tmp);
	xasprintf(&tmp, "%s/%s/%s", path, RC_USER_RUNLEVELS_FOLDER, RC_LEVEL_DEFAULT);
	if (mkdir(tmp, 0700) != 0 && errno != EEXIST) {
		eerrorx("mkdir: %s", strerror(errno));
	}
	free(tmp);
	free(path);

	path = rc_user_svcdir();
	if (mkdir(path, 0700) != 0 && errno != EEXIST) {
		eerrorx("mkdir: %s", strerror(errno));
	}
	free(path);

	setenv("RC_USER_SERVICES", "YES", 1);
}

char *
rc_user_sysconfdir(void)
{
	char *env, *path = NULL;
	if ((env = getenv("XDG_CONFIG_HOME"))) {
		xasprintf(&path, "%s", env);
	} else if ((env = getenv("HOME"))) {
		xasprintf(&path, "%s/.config/", env);
	} else {
		eerrorx("Both XDG_CONFIG_HOME and HOME unset.");
	}
	return path;
}

char *
rc_user_svcdir(void)
{
	char *env, *path = NULL;
	uid_t id;
	if ((env = getenv("XDG_RUNTIME_DIR"))) {
		xasprintf(&path, "%s%s", env, RC_USER_RUNTIME_FOLDER);
	} else {
		id = getuid();
		xasprintf(&path, "/tmp%s/%d/", RC_USER_RUNTIME_FOLDER, id);
	}
	return path;
}

#endif

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
	char path[PATH_MAX];
	RC_STRINGLIST *dirs;
	RC_STRING *d, *parent;
	const char *nextlevel;
	char *runlevel_dir = RC_RUNLEVELDIR;

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
#ifdef RC_USER_SERVICES
		if (rc_is_user()) {
			char *user_sysconf = rc_user_sysconfdir();
			xasprintf(&runlevel_dir, "%s/%s", user_sysconf, RC_USER_RUNLEVELS_FOLDER);
			free(user_sysconf);
		}
#endif

	snprintf(path, sizeof(path), "%s/%s", runlevel_dir, runlevel);
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

#ifdef RC_USER_SERVICES
		if (rc_is_user()) {
			free(runlevel_dir);
		}
#endif
}

bool
rc_runlevel_starting(void)
{
	char *rc_starting = RC_STARTING;
#ifdef RC_USER_SERVICES
	if (rc_is_user()) {
		char *user_svcdir = rc_user_svcdir();
		xasprintf(&rc_starting, "%s/%s", user_svcdir, RC_STARTING_FOLDER);
		free(user_svcdir);
	}
#endif
	return exists(rc_starting);
}

bool
rc_runlevel_stopping(void)
{
	char *rc_stopping = RC_STOPPING;
#ifdef RC_USER_SERVICES
	if (rc_is_user()) {
		char *user_svcdir = rc_user_svcdir();
		xasprintf(&rc_stopping, "%s/%s", user_svcdir, RC_STOPPING_FOLDER);
		free(user_svcdir);
	}
#endif
	return exists(rc_stopping);
}

RC_STRINGLIST *rc_runlevel_list(void)
{
	char *runlevel_dir = RC_RUNLEVELDIR;
	RC_STRINGLIST *runlevels;
#ifdef RC_USER_SERVICES
	char *user_sysconfdir;
	if (rc_is_user()) {
		user_sysconfdir = rc_user_sysconfdir();
		xasprintf(&runlevel_dir, "%s/%s", user_sysconfdir, RC_USER_RUNLEVELS_FOLDER);
	}
#endif
	 runlevels = ls_dir(runlevel_dir, LS_DIR);
#ifdef RC_USER_SERVICES
	if (rc_is_user()) {
		free(runlevel_dir);
	}
#endif
	return runlevels;
}

char *
rc_runlevel_get(void)
{
	FILE *fp;
	char *runlevel = NULL, *runlevel_path = RC_RUNLEVEL;
	size_t i;

#ifdef RC_USER_SERVICES
	if (rc_is_user()) {
		char *path = rc_user_svcdir();
		xasprintf(&runlevel_path, "%s/%s", path, RC_RUNLEVEL_FOLDER);
		free(path);
	}
#endif

	if ((fp = fopen(runlevel_path, "r"))) {
		runlevel = xmalloc(sizeof(char) * PATH_MAX);
		if (fgets(runlevel, PATH_MAX, fp)) {
			i = strlen(runlevel) - 1;
			if (runlevel[i] == '\n')
				runlevel[i] = 0;
		} else
			*runlevel = '\0';
		fclose(fp);
	}

#ifdef RC_USER_SERVICES
	if (rc_is_user()) {
		free(runlevel_path);
	}
#endif


	if (!runlevel || !*runlevel) {
		free(runlevel);
		runlevel = xstrdup(RC_LEVEL_SYSINIT);
	}

	return runlevel;
}

bool
rc_runlevel_set(const char *runlevel)
{
	char *runlevel_path = RC_RUNLEVEL;
	FILE *fp;

#ifdef RC_USER_SERVICES
	if (rc_is_user()) {
		char *path = rc_user_svcdir();
		xasprintf(&runlevel_path, "%s/%s", path, RC_RUNLEVEL_FOLDER);
		free(path);
	}
#endif

	fp = fopen(runlevel_path, "w");

	if (!fp)
		return false;
	fprintf(fp, "%s", runlevel);
	fclose(fp);


#ifdef RC_USER_SERVICES
	if (rc_is_user()) {
		free(runlevel_path);
	}
#endif
	return true;
}

bool
rc_runlevel_exists(const char *runlevel)
{
	char path[PATH_MAX];
	struct stat buf;

	if (!runlevel || strcmp(runlevel, "") == 0 || strcmp(runlevel, ".") == 0 ||
		strcmp(runlevel, "..") == 0)
		return false;

#ifdef RC_USER_SERVICES
	if (rc_is_user()) {
		char *user_path = rc_user_sysconfdir();
		snprintf(path, sizeof(path), "%s/%s/%s",
				user_path, RC_USER_RUNLEVELS_FOLDER, runlevel);
	} else {
#endif
		snprintf(path, sizeof(path), "%s/%s", RC_RUNLEVELDIR, runlevel);
#ifdef RC_USER_SERVICES
	}
#endif

	if (stat(path, &buf) == 0 && S_ISDIR(buf.st_mode))
		return true;
	return false;
}

bool
rc_runlevel_stack(const char *dst, const char *src)
{
	char d[PATH_MAX], s[PATH_MAX];
	char *runlevel_dir = RC_RUNLEVELDIR;

	if (!rc_runlevel_exists(dst) || !rc_runlevel_exists(src))
		return false;

#ifdef RC_USER_SERVICES
	if (rc_is_user()) {
		char *user_sysconf = rc_user_sysconfdir();
		xasprintf(&runlevel_dir, "%s/%s", user_sysconf, RC_USER_RUNLEVELS_FOLDER);
		free(user_sysconf);
	}
#endif

	snprintf(s, sizeof(s), "../%s", src);
	snprintf(d, sizeof(s), "%s/%s/%s", runlevel_dir, dst, src);

#ifdef RC_USER_SERVICES
	if (rc_is_user()) {
		free(runlevel_dir);
	}
#endif

	return (symlink(s, d) == 0 ? true : false);
}

bool
rc_runlevel_unstack(const char *dst, const char *src)
{
	char path[PATH_MAX];
	char *runlevel_dir = RC_RUNLEVELDIR;

#ifdef RC_USER_SERVICES
	if (rc_is_user()) {
		char *user_sysconf = rc_user_sysconfdir();
		xasprintf(&runlevel_dir, "%s/%s", user_sysconf, RC_USER_RUNLEVELS_FOLDER);
		free(user_sysconf);
	}
#endif

	snprintf(path, sizeof(path), "%s/%s/%s", runlevel_dir, dst, src);

#ifdef RC_USER_SERVICES
	if (rc_is_user()) {
		free(runlevel_dir);
	}
#endif
	return (unlink(path) == 0 ? true : false);
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

/* Resolve a service name to its full path */
char *
rc_service_resolve(const char *service)
{
	char buffer[PATH_MAX];
	char file[PATH_MAX];
	char *svcdir = RC_SVCDIR;
	int r;
	struct stat buf;

#ifdef RC_USER_SERVICES
	if (rc_is_user()) {
		svcdir = rc_user_svcdir();
	}
#endif

	if (!service)
		return NULL;

	if (service[0] == '/')
		return xstrdup(service);

	/* First check started services */
	snprintf(file, sizeof(file), "%s/%s/%s", svcdir, "started", service);
	if (lstat(file, &buf) || !S_ISLNK(buf.st_mode)) {
		snprintf(file, sizeof(file), "%s/%s/%s",
		    svcdir, "inactive", service);
		if (lstat(file, &buf) || !S_ISLNK(buf.st_mode))
			*file = '\0';
	}

#ifdef RC_USER_SERVICES
	if (rc_is_user()) {
		free(svcdir);
	}
#endif

	if (*file) {
		memset(buffer, 0, sizeof(buffer));
		r = readlink(file, buffer, sizeof(buffer)-1);
		if (r > 0)
			return xstrdup(buffer);
	}

#ifdef RC_USER_SERVICES
	/* If we're in user services mode, try user services*/
	if (rc_is_user()) {
		/* User defined services have preference to system provided */
		char *user_sysconfdir = rc_user_sysconfdir();
		snprintf(file, sizeof(file), "%s/%s/%s",
				user_sysconfdir, RC_USER_INITDIR_FOLDER, service);
		free(user_sysconfdir);
		if (stat(file, &buf) == 0)
			return xstrdup(file);

		snprintf(file, sizeof(file), "%s/%s",
				RC_SYS_USER_INITDIR, service);
		if (stat(file, &buf) == 0)
			return xstrdup(file);
	}
#endif

#ifdef RC_LOCAL_INITDIR
	/* Nope, so lets see if the user has written it */
	snprintf(file, sizeof(file), RC_LOCAL_INITDIR "/%s", service);
	if (stat(file, &buf) == 0)
		return xstrdup(file);
#endif

	/* System scripts take precedence over 3rd party ones */
	snprintf(file, sizeof(file), RC_INITDIR "/%s", service);
	if (stat(file, &buf) == 0)
		return xstrdup(file);

#ifdef RC_PKG_INITDIR
	/* Check RC_PKG_INITDIR */
	snprintf(file, sizeof(file), RC_PKG_INITDIR "/%s", service);
	if (stat(file, &buf) == 0)
		return xstrdup(file);
#endif

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
	char file[PATH_MAX];
	char *runlevel_dir = RC_RUNLEVELDIR;

#ifdef RC_USER_SERVICES
		if (rc_is_user()) {
			char *user_sysconf = rc_user_sysconfdir();
			xasprintf(&runlevel_dir, "%s/%s", user_sysconf, RC_USER_RUNLEVELS_FOLDER);
			free(user_sysconf);
		}
#endif
	snprintf(file, sizeof(file), "%s/%s/%s",
	    runlevel_dir, runlevel, basename_c(service));

#ifdef RC_USER_SERVICES
		if (rc_is_user()) {
			free(runlevel_dir);
		}
#endif

	return exists(file);
}

bool
rc_service_mark(const char *service, const RC_SERVICE state)
{
	char file[PATH_MAX];
	int i = 0;
	int skip_state = -1;
	const char *base;
	char *init = rc_service_resolve(service);
	bool skip_wasinactive = false;
	int s;
	char was[PATH_MAX];
	RC_STRINGLIST *dirs;
	RC_STRING *dir;
	int serrno;
	char *svcdir = RC_SVCDIR;
#ifdef RC_USER_SERVICES
	if (rc_is_user()) {
		svcdir = rc_user_svcdir();
	}
#endif

	if (!init)
		return false;

	base = basename_c(service);
	if (state != RC_SERVICE_STOPPED) {
		if (!exists(init)) {
			free(init);
			return false;
		}

		snprintf(file, sizeof(file), "%s/%s/%s",
		    svcdir,rc_parse_service_state(state), base);
		if (exists(file))
			unlink(file);
		i = symlink(init, file);
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
			snprintf(file, sizeof(file), "%s/%s/%s",
			    svcdir, rc_service_state_names[i].name, base);
			if (exists(file)) {
				if ((state == RC_SERVICE_STARTING ||
					state == RC_SERVICE_STOPPING) &&
				    s == RC_SERVICE_INACTIVE)
				{
					snprintf(was, sizeof(was),
					    "%s/%s/%s",
						svcdir,
					    rc_parse_service_state(RC_SERVICE_WASINACTIVE),
					    base);
					if (symlink(init, was) == -1) {
						free(init);
						return false;
					}
					skip_wasinactive = true;
				}
				if (unlink(file) == -1) {
					free(init);
					return false;
				}
			}
		}
	}

	/* Remove the exclusive state if we're inactive */
	if (state == RC_SERVICE_STARTED ||
	    state == RC_SERVICE_STOPPED ||
	    state == RC_SERVICE_INACTIVE)
	{
		snprintf(file, sizeof(file), "%s/%s/%s",
		    svcdir, "exclusive", base);
		unlink(file);
	}

	/* Remove any options and daemons the service may have stored */
	if (state == RC_SERVICE_STOPPED) {
		snprintf(file, sizeof(file), "%s/%s/%s",
		    svcdir, "options", base);
		rm_dir(file, true);

		snprintf(file, sizeof(file), "%s/%s/%s",
		   svcdir, "daemons", base);
		rm_dir(file, true);

		rc_service_schedule_clear(service);
	}

	/* These are final states, so remove us from scheduled */
	if (state == RC_SERVICE_STARTED || state == RC_SERVICE_STOPPED) {
		snprintf(file, sizeof(file), "%s/%s", svcdir, "scheduled");
		dirs = ls_dir(file, 0);
		TAILQ_FOREACH(dir, dirs, entries) {
			snprintf(was, sizeof(was), "%s/%s/%s",
			    file, dir->value, base);
			unlink(was);

			/* Try and remove the dir; we don't care about errors */
			snprintf(was, sizeof(was), "%s/%s", file, dir->value);
			serrno = errno;
			rmdir(was);
			errno = serrno;
		}
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
	char file[PATH_MAX];
	RC_STRINGLIST *dirs;
	RC_STRING *dir;
	const char *base = basename_c(service);
	char *svcdir = RC_SVCDIR;
#ifdef RC_USER_SERVICES
	if (rc_is_user()) {
		svcdir = rc_user_svcdir();
	}
#endif

	for (i = 0; rc_service_state_names[i].name; i++) {
		snprintf(file, sizeof(file), "%s/%s/%s",
		    svcdir, rc_service_state_names[i].name, base);
		if (exists(file)) {
			if (rc_service_state_names[i].state <= 0x10)
				state = rc_service_state_names[i].state;
			else
				state |= rc_service_state_names[i].state;
		}
	}

	if (state & RC_SERVICE_STARTED) {
		if (rc_service_daemons_crashed(service) && errno != EACCES)
			state |= RC_SERVICE_CRASHED;
	}
	if (state & RC_SERVICE_STOPPED) {
		char *scheduled;
		xasprintf(&scheduled, "%s/%s", svcdir, "/scheduled");
		dirs = ls_dir(scheduled, 0);
		free(scheduled);
		TAILQ_FOREACH(dir, dirs, entries) {
			snprintf(file, sizeof(file),
				"%s/scheduled/%s/%s",
				svcdir, dir->value, service);
			if (exists(file)) {
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
	char file[PATH_MAX];
	char *svcdir = RC_SVCDIR;
#ifdef RC_USER_SERVICES
	if (rc_is_user()) {
		svcdir = rc_user_svcdir();
	}
#endif

	snprintf(file, sizeof(file), "%s/options/%s/%s",
	    svcdir, service, option);
	rc_getfile(file, &buffer, &len);

	return buffer;
}

bool
rc_service_value_set(const char *service, const char *option,
    const char *value)
{
	FILE *fp;
	char file[PATH_MAX];
	char *p = file;
	char *svcdir = RC_SVCDIR;
#ifdef RC_USER_SERVICES
	if (rc_is_user()) {
		svcdir = rc_user_svcdir();
	}
#endif

	p += snprintf(file, sizeof(file), "%s/options/%s", svcdir, service);
	if (mkdir(file, 0755) != 0 && errno != EEXIST)
		return false;

	snprintf(p, sizeof(file) - (p - file), "/%s", option);
	if (value) {
		if (!(fp = fopen(file, "w")))
			return false;
		fprintf(fp, "%s", value);
		fclose(fp);
	} else {
		unlink(file);
	}
		return true;
}


bool
rc_service_schedule_start(const char *service, const char *service_to_start)
{
	char file[PATH_MAX];
	char *p = file;
	char *init;
	bool retval;
	char *svcdir = RC_SVCDIR;
#ifdef RC_USER_SERVICES
	if (rc_is_user()) {
		svcdir = rc_user_svcdir();
	}
#endif

	/* service may be a provided service, like net */
	if (!service || !rc_service_exists(service_to_start))
		return false;

	p += snprintf(file, sizeof(file), "%s/scheduled/%s",
	    svcdir, basename_c(service));
	if (mkdir(file, 0755) != 0 && errno != EEXIST)
		return false;

	init = rc_service_resolve(service_to_start);
	snprintf(p, sizeof(file) - (p - file),
	    "/%s", basename_c(service_to_start));
	retval = (exists(file) || symlink(init, file) == 0);
	free(init);
	return retval;
}

bool
rc_service_schedule_clear(const char *service)
{
	char dir[PATH_MAX];
	char *svcdir = RC_SVCDIR;
#ifdef RC_USER_SERVICES
	if (rc_is_user()) {
		svcdir = rc_user_svcdir();
	}
#endif

	snprintf(dir, sizeof(dir), "%s/scheduled/%s",
	    svcdir, basename_c(service));
	if (!rm_dir(dir, true) && errno == ENOENT)
		return true;
	return false;
}

RC_STRINGLIST *
rc_services_in_runlevel(const char *runlevel)
{
	char dir[PATH_MAX];
	RC_STRINGLIST *list = NULL;
	char *runlevel_dir = RC_RUNLEVELDIR;

	if (!runlevel) {
#ifdef RC_PKG_INITDIR
		RC_STRINGLIST *pkg = ls_dir(RC_PKG_INITDIR, LS_INITD);
#endif
#ifdef RC_LOCAL_INITDIR
		RC_STRINGLIST *local = ls_dir(RC_LOCAL_INITDIR, LS_INITD);
#endif
#ifdef RC_USER_SERVICES
	RC_STRINGLIST *local_user;
	RC_STRINGLIST *sys_user;
	char *user_sysconf;
	char *user_initdir;
	if (rc_is_user()) {
		user_sysconf = rc_user_sysconfdir();
		xasprintf(&user_initdir, "%s/%s", user_sysconf, RC_USER_INITDIR_FOLDER);

		local_user = ls_dir(user_initdir, LS_INITD);
		sys_user = ls_dir(RC_SYS_USER_INITDIR, LS_INITD);

		list = rc_stringlist_new();
	} else {
#endif

		list = ls_dir(RC_INITDIR, LS_INITD);

#ifdef RC_USER_SERVICES
	}
#endif

#ifdef RC_PKG_INITDIR
		TAILQ_CONCAT(list, pkg, entries);
		rc_stringlist_free(pkg);
#endif
#ifdef RC_LOCAL_INITDIR
		TAILQ_CONCAT(list, local, entries);
		rc_stringlist_free(local);
#endif
#ifdef RC_USER_SERVICES
	if (rc_is_user()) {
		TAILQ_CONCAT(list, local_user, entries);
		TAILQ_CONCAT(list, sys_user, entries);
		rc_stringlist_free(local_user);
		rc_stringlist_free(sys_user);
		free(user_sysconf);
		free(user_initdir);
	}
#endif
		return list;
	}

	/* These special levels never contain any services */
	if (strcmp(runlevel, RC_LEVEL_SINGLE) != 0) {
#ifdef RC_USER_SERVICES
		if (rc_is_user()) {
			char *user_sysconf = rc_user_sysconfdir();
			xasprintf(&runlevel_dir, "%s/%s", user_sysconf, RC_USER_RUNLEVELS_FOLDER);
			free(user_sysconf);
		}
#endif
		snprintf(dir, sizeof(dir), "%s/%s", runlevel_dir, runlevel);
		list = ls_dir(dir, LS_INITD);

#ifdef RC_USER_SERVICES
		if (rc_is_user()) {
			free(runlevel_dir);
		}
#endif

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
	char dir[PATH_MAX];
	char *p = dir;
	char *svcdir = RC_SVCDIR;

#ifdef RC_USER_SERVICES
	if (rc_is_user()) {
		svcdir = rc_user_svcdir();
	}
#endif

	p += snprintf(dir, sizeof(dir), "%s/%s",
	    svcdir, rc_parse_service_state(state));

	if (state != RC_SERVICE_SCHEDULED)
		return ls_dir(dir, LS_INITD);

	dirs = ls_dir(dir, 0);
	list = rc_stringlist_new();
	if (!dirs)
		return list;

	TAILQ_FOREACH(d, dirs, entries) {
		snprintf(p, sizeof(dir) - (p - dir), "/%s", d->value);
		services = ls_dir(dir, LS_INITD);
		if (services) {
			TAILQ_CONCAT(list, services, entries);
			rc_stringlist_free(services);
		}
	}
	rc_stringlist_free(dirs);

#ifdef RC_USER_SERVICES
	if (rc_is_user()) {
		free(svcdir);
	}
#endif

	return list;
}

bool
rc_service_add(const char *runlevel, const char *service)
{
	bool retval;
	char *init;
	char file[PATH_MAX];
	char path[MAXPATHLEN] = { '\0' };
	char binit[PATH_MAX];
	char *runlevel_dir = RC_RUNLEVELDIR;
	char *i;
#ifdef RC_USER_SERVICES
	if (rc_is_user()) {
		char *user_sysconf = rc_user_sysconfdir();
		xasprintf(&runlevel_dir, "%s/%s", user_sysconf, RC_USER_RUNLEVELS_FOLDER);
		free(user_sysconf);
	}
#endif

	if (!rc_runlevel_exists(runlevel)) {
		errno = ENOENT;
		return false;
	}

	if (rc_service_in_runlevel(service, runlevel)) {
		errno = EEXIST;
		return false;
	}

	i = init = rc_service_resolve(service);
	snprintf(file, sizeof(file), "%s/%s/%s",
	    runlevel_dir, runlevel, basename_c(service));

#ifdef RC_USER_SERVICES
	if (rc_is_user()) {
		free(runlevel_dir);
	}
#endif

	/* We need to ensure that only things in /etc/init.d are added
	 * to the boot runlevel */
	if (strcmp(runlevel, RC_LEVEL_BOOT) == 0) {
		if (realpath(dirname(init), path) == NULL) {
			free(init);
			return false;
		}
		if (strcmp(path, RC_INITDIR) != 0) {
			free(init);
			errno = EPERM;
			return false;
		}
		snprintf(binit, sizeof(binit), RC_INITDIR "/%s", service);
		i = binit;
	}

	retval = (symlink(i, file) == 0);
	free(init);
	return retval;
}

bool
rc_service_delete(const char *runlevel, const char *service)
{
	char file[PATH_MAX];
	char *runlevel_dir = RC_RUNLEVELDIR;

#ifdef RC_USER_SERVICES
	if (rc_is_user()) {
		char *user_sysconf = rc_user_sysconfdir();
		xasprintf(&runlevel_dir, "%s/%s", user_sysconf, RC_USER_RUNLEVELS_FOLDER);
		free(user_sysconf);
	}
#endif

	snprintf(file, sizeof(file), "%s/%s/%s",
	    runlevel_dir, runlevel, basename_c(service));

#ifdef RC_USER_SERVICES
	if (rc_is_user()) {
		free(runlevel_dir);
	}
#endif

	if (unlink(file) == 0)
		return true;
	return false;
}

RC_STRINGLIST *
rc_services_scheduled_by(const char *service)
{
	RC_STRINGLIST *dirs;
	RC_STRINGLIST *list = rc_stringlist_new();
	RC_STRING *dir;
	char file[PATH_MAX];
	char *scheduled;
	char *svcdir = RC_SVCDIR;

#ifdef RC_USER_SERVICES
	if (rc_is_user()) {
		svcdir = rc_user_svcdir();
	}
#endif

	xasprintf(&scheduled, "%s/%s", svcdir, "/scheduled");
	dirs = ls_dir(scheduled, 0);
	free(scheduled);
	TAILQ_FOREACH(dir, dirs, entries) {
		snprintf(file, sizeof(file), "%s/scheduled/%s/%s",
		    svcdir, dir->value, service);
		if (exists(file))
			rc_stringlist_add(list, file);
	}
	rc_stringlist_free(dirs);
	return list;
}

RC_STRINGLIST *
rc_services_scheduled(const char *service)
{
	char dir[PATH_MAX];
	char *svcdir = RC_SVCDIR;

#ifdef RC_USER_SERVICES
	if (rc_is_user()) {
		svcdir = rc_user_svcdir();
	}
#endif

	snprintf(dir, sizeof(dir), "%s/scheduled/%s",
	    svcdir, basename_c(service));
	return ls_dir(dir, LS_INITD);
}
