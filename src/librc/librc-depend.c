/*
 *  librc-depend
 *  rc service dependency and ordering
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

#include <sys/utsname.h>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "queue.h"
#include "librc.h"
#include "helpers.h"
#include "misc.h"

#define GENDEP          RC_LIBEXECDIR "/sh/gendepends.sh"

static const char *bootlevel = NULL;

static char *
get_shell_value(char *string)
{
	char *p = string;
	char *e;

	if (!string)
		return NULL;

	if (*p == '\'')
		p++;

	e = p + strlen(p) - 1;
	if (*e == '\n')
		*e-- = 0;
	if (*e == '\'')
		*e-- = 0;

	if (*p != 0)
		return p;

	return NULL;
}

void
rc_deptree_free(RC_DEPTREE *deptree)
{
	RC_DEPINFO *di;
	RC_DEPINFO *di_save;

	if (!deptree)
		return;

	TAILQ_FOREACH_SAFE(di, deptree, entries, di_save) {
		for (size_t i = 0; i < RC_DEPTYPE_MAX; i++) {
			struct rc_dep *dt;
			struct rc_dep *dt_save;

			TAILQ_FOREACH_SAFE(dt, &di->depends[i], entries, dt_save) {
				TAILQ_REMOVE(&di->depends[i], dt, entries);
				free(dt);
			}
		}
		TAILQ_REMOVE(deptree, di, entries);
		free(di->service);
		free(di);
	}

	/* Use free() here since rc_deptree_free should not call itself */
	free(deptree);
}

static RC_DEPINFO *
get_depinfo(const RC_DEPTREE *deptree, const char *service)
{
	RC_DEPINFO *di;
	if (deptree) {
		TAILQ_FOREACH(di, deptree, entries)
			if (strcmp(di->service, service) == 0)
				return di;
	}
	return NULL;
}

static RC_DEPINFO *
make_depinfo(RC_DEPTREE *deptree, const char *service)
{
	RC_DEPINFO *depinfo = xmalloc(sizeof(*depinfo));
	for (size_t i = 0; i < RC_DEPTYPE_MAX; i++)
		TAILQ_INIT(&depinfo->depends[i]);
	depinfo->service = xstrdup(service);
	TAILQ_INSERT_TAIL(deptree, depinfo, entries);

	return depinfo;
}

const char *deptype_map[RC_DEPTYPE_MAX] = {
	[RC_DEPTYPE_INEED] = "ineed",
	[RC_DEPTYPE_NEEDSME] = "needsme",
	[RC_DEPTYPE_IUSE] = "iuse",
	[RC_DEPTYPE_USESME] = "usesme",
	[RC_DEPTYPE_IWANT] = "iwant",
	[RC_DEPTYPE_WANTSME] = "wantsme",
	[RC_DEPTYPE_IAFTER] = "iafter",
	[RC_DEPTYPE_AFTERME] = "afterme",
	[RC_DEPTYPE_IBEFORE] = "ibefore",
	[RC_DEPTYPE_BEFOREME] = "beforeme",
	[RC_DEPTYPE_IPROVIDE] = "iprovide",
	[RC_DEPTYPE_PROVIDEDBY] = "providedby",
	[RC_DEPTYPE_BROKEN] = "broken",
	[RC_DEPTYPE_KEYWORD] = "keyword",
};

enum rc_deptype
rc_deptype_parse(const char *type)
{
	for (size_t i = 0; i < RC_DEPTYPE_MAX; i++)
		if (deptype_map[i] && strcmp(type, deptype_map[i]) == 0)
			return i;
	return RC_DEPTYPE_INVALID;
}

static struct rc_dep *
make_dep(RC_DEPINFO *service)
{
	struct rc_dep *dep = xmalloc(sizeof(*dep));
	dep->service = service;
	return dep;
}

#ifdef HAVE_MALLOC_EXTENDED_ATTRIBUTE
__attribute__ ((malloc (rc_deptree_free, 1)))
#endif
static RC_DEPTREE *
make_deptree(void) {
	RC_DEPTREE *deptree = xmalloc(sizeof(*deptree));
	TAILQ_INIT(deptree);
	return deptree;
}

RC_DEPTREE *
rc_deptree_load(void) {
	char *deptree_cache;
	RC_DEPTREE *deptree;

	xasprintf(&deptree_cache, "%s/deptree", rc_svcdir());
	deptree = rc_deptree_load_file(deptree_cache);
	free(deptree_cache);

	return deptree;
}

RC_DEPTREE *
rc_deptree_load_file(const char *deptree_file)
{
	FILE *fp;
	RC_DEPTREE *deptree;
	RC_DEPINFO *depinfo = NULL, *depsvc;
	struct rc_dep *dependency = NULL;
	enum rc_deptype type_idx;
	char *line = NULL;
	size_t size;
	char *type;
	char *p;
	char *e;
	int i;

	if (!(fp = fopen(deptree_file, "r")))
		return NULL;

	deptree = make_deptree();
	while (xgetline(&line, &size, fp) != -1) {
		p = line;
		e = strsep(&p, "_");
		if (!e || strcmp(e, "depinfo") != 0)
			continue;

		e = strsep(&p, "_");
		if (!e || sscanf(e, "%d", &i) != 1)
			continue;
		if (!(type = strsep(&p, "_=")))
			continue;

		if (strcmp(type, "service") == 0) {
			/* Sanity */
			e = get_shell_value(p);
			if (!e || *e == '\0')
				continue;
			if (!(depinfo = get_depinfo(deptree, e)))
				depinfo = make_depinfo(deptree, e);
			dependency = NULL;
			continue;
		}

		if ((type_idx = rc_deptype_parse(type)) == RC_DEPTYPE_INVALID)
			continue;

		e = strsep(&p, "=");
		if (!e || sscanf(e, "%d", &i) != 1)
			continue;
		/* Sanity */
		e = get_shell_value(p);
		if (!e || *e == '\0')
			continue;
		if (!(depsvc = get_depinfo(deptree, e)))
			depsvc = make_depinfo(deptree, e);
		dependency = xmalloc(sizeof(*dependency));
		dependency->service = depsvc;
		TAILQ_INSERT_TAIL(&depinfo->depends[type_idx], dependency, entries);
	}
	fclose(fp);
	free(line);

	return deptree;
}

static bool
valid_service(const char *runlevel, const char *service, enum rc_deptype type)
{
	RC_SERVICE state;

	if (!runlevel)
		return true;

	switch (type) {
	case RC_DEPTYPE_INEED:
	case RC_DEPTYPE_NEEDSME:
	case RC_DEPTYPE_IWANT:
	case RC_DEPTYPE_WANTSME:
		return true;
	default:
		break;
	}

	if (rc_service_in_runlevel(service, runlevel))
		return true;
	if (type == RC_DEPTYPE_IAFTER && strcmp(runlevel, RC_LEVEL_SHUTDOWN) == 0)
		    return false;
	if (strcmp(runlevel, RC_LEVEL_SYSINIT) == 0)
		    return false;
	if (strcmp(runlevel, bootlevel) != 0) {
		if (rc_service_in_runlevel(service, bootlevel))
			return true;
	}

	state = rc_service_state(service);
	if (state & RC_SERVICE_HOTPLUGGED || state & RC_SERVICE_STARTED)
		return true;

	return false;
}

static bool
get_provided1(const char *runlevel, RC_STRINGLIST *providers,
		const struct rc_deplist *deplist, const char *level, bool hotplugged, RC_SERVICE state)
{
	RC_SERVICE st;
	struct rc_dep *service;
	bool retval = false;
	bool ok;
	const char *svc;

	TAILQ_FOREACH(service, deplist, entries) {
		ok = true;
		svc = service->service->service;
		st = rc_service_state(svc);

		if (level)
			ok = rc_service_in_runlevel(svc, level);
		else if (hotplugged)
			ok = (st & RC_SERVICE_HOTPLUGGED &&
			      !rc_service_in_runlevel(svc, runlevel) &&
			      !rc_service_in_runlevel(svc, bootlevel));
		if (!ok)
			continue;
		switch (state) {
			case RC_SERVICE_STARTED:
				ok = (st & RC_SERVICE_STARTED);
				break;
			case RC_SERVICE_INACTIVE:
			case RC_SERVICE_STARTING:
			case RC_SERVICE_STOPPING:
				ok = (st & RC_SERVICE_STARTING ||
				      st & RC_SERVICE_STOPPING ||
				      st & RC_SERVICE_INACTIVE);
				break;
			default:
				break;
		}
		if (!ok)
			continue;
		retval = true;
		rc_stringlist_add(providers, svc);
	}

	return retval;
}

/* Work out if a service is provided by another service.
   For example metalog provides logger.
   We need to be able to handle syslogd providing logger too.
   We do this by checking whats running, then what's starting/stopping,
   then what's run in the runlevels and finally alphabetical order.

   If there are any bugs in rc-depend, they will probably be here as
   provided dependancy can change depending on runlevel state.
   */
static RC_STRINGLIST *
get_provided(const RC_DEPINFO *depinfo, const char *runlevel, int options)
{
	const struct rc_deplist *deplist = &depinfo->depends[RC_DEPTYPE_PROVIDEDBY];
	RC_STRINGLIST *providers = rc_stringlist_new();
	struct rc_dep *dep;

	if (TAILQ_EMPTY(deplist))
		return providers;

	/* If we are stopping then all depends are true, regardless of state.
	   This is especially true for net services as they could force a restart
	   of the local dns resolver which may depend on net. */
	if (options & RC_DEP_STOP) {
		TAILQ_FOREACH(dep, deplist, entries)
			rc_stringlist_add(providers, dep->service->service);
		return providers;
	}

	/* If we're strict or starting, then only use what we have in our
	 * runlevel and bootlevel. If we starting then check hotplugged too. */
	if (options & RC_DEP_STRICT || options & RC_DEP_START) {
		TAILQ_FOREACH(dep, deplist, entries) {
			const char *svcname = dep->service->service;
			if (rc_service_in_runlevel(svcname, runlevel) || rc_service_in_runlevel(svcname, bootlevel) ||
					(options & RC_DEP_START && rc_service_state(svcname) & RC_SERVICE_HOTPLUGGED))
				rc_stringlist_add(providers, svcname);
		}
		if (TAILQ_FIRST(providers))
			return providers;
	}

	/* OK, we're not strict or there were no services in our runlevel.
	 * This is now where the logic gets a little fuzzy :)
	 * If there is >1 running service then we return NULL.
	 * We do this so we don't hang around waiting for inactive services and
	 * our need has already been satisfied as it's not strict.
	 * We apply this to these states in order:-
	 *     started, starting | stopping | inactive, stopped
	 * Our sub preference in each of these is in order:-
	 *     runlevel, hotplugged, bootlevel, any
	 */
#define DO \
	if (TAILQ_FIRST(providers)) { \
		if (TAILQ_NEXT(TAILQ_FIRST(providers), entries)) { \
			rc_stringlist_free(providers); \
			providers = rc_stringlist_new(); \
		} \
		return providers; \
	}

	/* Anything running has to come first */
	if (get_provided1(runlevel, providers, deplist, runlevel, false, RC_SERVICE_STARTED))
	{ DO }
	if (get_provided1(runlevel, providers, deplist, NULL, true, RC_SERVICE_STARTED))
	{ DO }
	if (bootlevel && strcmp(runlevel, bootlevel) != 0 &&
	    get_provided1(runlevel, providers, deplist, bootlevel, false, RC_SERVICE_STARTED))
	{ DO }
	if (get_provided1(runlevel, providers, deplist, NULL, false, RC_SERVICE_STARTED))
	{ DO }

	/* Check starting services */
	if (get_provided1(runlevel, providers, deplist, runlevel, false, RC_SERVICE_STARTING))
		return providers;
	if (get_provided1(runlevel, providers, deplist, NULL, true, RC_SERVICE_STARTING))
		return providers;
	if (bootlevel && strcmp(runlevel, bootlevel) != 0 &&
	    get_provided1(runlevel, providers, deplist, bootlevel, false, RC_SERVICE_STARTING))
	    return providers;
	if (get_provided1(runlevel, providers, deplist, NULL, false, RC_SERVICE_STARTING))
		return providers;

	/* Nothing started then. OK, lets get the stopped services */
	if (get_provided1(runlevel, providers, deplist, runlevel, false, RC_SERVICE_STOPPED))
		return providers;
	if (get_provided1(runlevel, providers, deplist, NULL, true, RC_SERVICE_STOPPED))
	{ DO }
	if (bootlevel && (strcmp(runlevel, bootlevel) != 0) &&
	    get_provided1(runlevel, providers, deplist, bootlevel, false, RC_SERVICE_STOPPED))
		return providers;

	/* Still nothing? OK, list our first provided service. */
	if (!TAILQ_EMPTY(deplist))
		rc_stringlist_add(providers, TAILQ_FIRST(deplist)->service->service);

	return providers;
}

static void
visit_service(const RC_DEPTREE *deptree, enum rc_deptype types, RC_STRINGLIST *sorted,
		RC_DEPINFO *depinfo, const char *runlevel, int options, int idx)
{
	RC_DEPINFO *di;
	RC_STRINGLIST *provided;
	RC_STRING *p;
	const char *svcname;

	/* Check if we have already visited this service or not */
	if (depinfo->visited == idx)
		return;
	depinfo->visited = idx;

	for (size_t i = 0; i < RC_DEPTYPE_MAX; i++) {
		struct rc_dep *dt;
		if (!((types >> i) & 1))
			continue;

		TAILQ_FOREACH(dt, &depinfo->depends[i], entries) {
			svcname = dt->service->service;
			if (!(options & RC_DEP_TRACE) || i == RC_DEPTYPE_IPROVIDE) {
				rc_stringlist_add(sorted, svcname);
				continue;
			}

			if (!(di = get_depinfo(deptree, svcname)))
				continue;
			provided = get_provided(di, runlevel, options);

			if (TAILQ_FIRST(provided)) {
				TAILQ_FOREACH(p, provided, entries) {
					di = get_depinfo(deptree, p->value);
					if (di && valid_service(runlevel, di->service, i))
						visit_service(deptree, types, sorted, di, runlevel, options | RC_DEP_TRACE, idx);
				}
			} else if (di && valid_service(runlevel, svcname, i)) {
				visit_service(deptree, types, sorted, di, runlevel, options | RC_DEP_TRACE, idx);
			}

			rc_stringlist_free(provided);
		}
	}

	/* Now visit the stuff we provide for */
	if (options & RC_DEP_TRACE) {
		struct rc_dep *dt;
		TAILQ_FOREACH(dt, &depinfo->depends[RC_DEPTYPE_IPROVIDE], entries) {
			if (!(di = get_depinfo(deptree, dt->service->service)))
				continue;
			provided = get_provided(di, runlevel, options);
			TAILQ_FOREACH(p, provided, entries)
				if (strcmp(p->value, depinfo->service) == 0) {
					visit_service(deptree, types, sorted, di, runlevel, options | RC_DEP_TRACE, idx);
					break;
				}
			rc_stringlist_free(provided);
		}
	}

	/* We've visited everything we need, so add ourselves unless we
	   are also the service calling us or we are provided by something */
	svcname = getenv("RC_SVCNAME");
	if (!svcname || strcmp(svcname, depinfo->service) != 0) {
		if (TAILQ_EMPTY(&depinfo->depends[RC_DEPTYPE_PROVIDEDBY]))
			rc_stringlist_add(sorted, depinfo->service);
	}
}

RC_STRINGLIST *
rc_deptree_depend(const RC_DEPTREE *deptree, const char *service, enum rc_deptype type)
{
	RC_DEPINFO *di;
	struct rc_dep *dt;
	RC_STRINGLIST *svcs = rc_stringlist_new();

	if (type >= RC_DEPTYPE_MAX || !(di = get_depinfo(deptree, service)) || TAILQ_EMPTY(&di->depends[type]))
	{
		errno = ENOENT;
		return svcs;
	}

	/* For consistency, we copy the array */
	TAILQ_FOREACH(dt, &di->depends[type], entries)
		rc_stringlist_add(svcs, dt->service->service);
	return svcs;
}

RC_STRINGLIST *
rc_deptree_depends(const RC_DEPTREE *deptree, enum rc_deptype types,
		   const RC_STRINGLIST *services, const char *runlevel, int options)
{
	RC_STRINGLIST *sorted = rc_stringlist_new();
	const RC_STRING *service;
	RC_DEPINFO *di;
	static int idx = 2;

	bootlevel = getenv("RC_BOOTLEVEL");
	if (!bootlevel)
		bootlevel = RC_LEVEL_BOOT;
	TAILQ_FOREACH(service, services, entries) {
		if (!(di = get_depinfo(deptree, service->value))) {
			errno = ENOENT;
			continue;
		}
		visit_service(deptree, types, sorted, di, runlevel, options, idx++);
	}
	return sorted;
}

RC_STRINGLIST *
rc_deptree_order(const RC_DEPTREE *deptree, const char *runlevel, int options)
{
	RC_STRINGLIST *list;
	RC_STRINGLIST *list2;
	enum rc_deptype types;
	RC_STRINGLIST *services;

	bootlevel = getenv("RC_BOOTLEVEL");
	if (!bootlevel)
		bootlevel = RC_LEVEL_BOOT;

	/* When shutting down, list all running services */
	if (strcmp(runlevel, RC_LEVEL_SINGLE) == 0 ||
	    strcmp(runlevel, RC_LEVEL_SHUTDOWN) == 0)
	{
		list = rc_services_in_state(RC_SERVICE_STARTED);
		list2 = rc_services_in_state(RC_SERVICE_INACTIVE);
		TAILQ_CONCAT(list, list2, entries);
		free(list2);
		list2 = rc_services_in_state(RC_SERVICE_STARTING);
		TAILQ_CONCAT(list, list2, entries);
		free(list2);
	} else {
		list = rc_services_in_runlevel(RC_LEVEL_SYSINIT);
		if (strcmp(runlevel, RC_LEVEL_SYSINIT) != 0) {
			list2 = rc_services_in_runlevel(runlevel);
			TAILQ_CONCAT(list, list2, entries);
			free(list2);
			list2 = rc_services_in_state(RC_SERVICE_HOTPLUGGED);
			TAILQ_CONCAT(list, list2, entries);
			free(list2);
			/* If we're not the boot runlevel then add that too */
			if (strcmp(runlevel, bootlevel) != 0) {
				list2 = rc_services_in_runlevel(bootlevel);
				TAILQ_CONCAT(list, list2, entries);
				free(list2);
			}
		}
	}

	/* Now we have our lists, we need to pull in any dependencies
	   and order them */
	types = RC_DEP(INEED) | RC_DEP(IUSE) | RC_DEP(IWANT) | RC_DEP(IAFTER);
	services = rc_deptree_depends(deptree, types, list, runlevel, RC_DEP_STRICT | RC_DEP_TRACE | options);

	rc_stringlist_free(list);
	return services;
}


/* Given a time, recurse the target path to find out if there are
   any older (or newer) files.   If false, sets the time to the
   oldest (or newest) found.
*/
static bool
deep_mtime_check(const char *target, bool newer,
	    time_t *rel, char *file)
{
	struct stat buf;
	bool retval = true;
	DIR *dp;
	struct dirent *d;
	char path[PATH_MAX];
	int serrno = errno;

	/* If target does not exist, return true to mimic shell test */
	if (stat(target, &buf) != 0)
		return true;

	if (newer) {
		if (*rel < buf.st_mtime) {
			retval = false;

			if (file)
				strlcpy(file, target, PATH_MAX);
			*rel = buf.st_mtime;
		}
	} else {
		if (*rel > buf.st_mtime) {
			retval = false;

			if (file)
				strlcpy(file, target, PATH_MAX);
			*rel = buf.st_mtime;
		}
	}

	/* If not a dir then reset errno */
	if (!(dp = opendir(target))) {
		errno = serrno;
		return retval;
	}

	/* Check all the entries in the dir */
	while ((d = readdir(dp))) {
		if (d->d_name[0] == '.')
			continue;
		snprintf(path, sizeof(path), "%s/%s", target, d->d_name);
		if (!deep_mtime_check(path, newer, rel, file)) {
			retval = false;
		}
	}
	closedir(dp);
	return retval;
}

/* Recursively check if target is older/newer than source.
 * If false, return the filename and most different time (if
 * the return value arguments are non-null).
 */
static bool
mtime_check(const char *source, const char *target, bool newer,
	    time_t *rel, char *file)
{
	struct stat buf;
	time_t mtime;
	bool retval = true;

	/* We have to exist */
	if (stat(source, &buf) != 0)
		return false;
	mtime = buf.st_mtime;

	retval = deep_mtime_check(target,newer,&mtime,file);
	if (rel) {
		*rel = mtime;
	}
	return retval;
}

bool
rc_newer_than(const char *source, const char *target,
	      time_t *newest, char *file)
{

	return mtime_check(source, target, true, newest, file);
}

bool
rc_older_than(const char *source, const char *target,
	      time_t *oldest, char *file)
{
	return mtime_check(source, target, false, oldest, file);
}

typedef struct deppair
{
	enum rc_deptype depend;
	enum rc_deptype addto;
} DEPPAIR;

static const DEPPAIR deppairs[] = {
	{ RC_DEPTYPE_INEED,	RC_DEPTYPE_NEEDSME },
	{ RC_DEPTYPE_IUSE,	RC_DEPTYPE_USESME },
	{ RC_DEPTYPE_IWANT,	RC_DEPTYPE_WANTSME },
	{ RC_DEPTYPE_IAFTER,	RC_DEPTYPE_IBEFORE },
	{ RC_DEPTYPE_IBEFORE,	RC_DEPTYPE_IAFTER },
	{ RC_DEPTYPE_IPROVIDE,	RC_DEPTYPE_PROVIDEDBY },
};

static const char *const depdirs[] =
{
	"",
	"starting",
	"started",
	"stopping",
	"inactive",
	"wasinactive",
	"failed",
	"hotplugged",
	"daemons",
	"options",
	"exclusive",
	"scheduled",
	"init.d",
	"tmp",
	NULL
};

bool
rc_deptree_update_needed(time_t *newest, char *file)
{
	bool newer = false;
	RC_STRINGLIST *config;
	RC_STRING *s;
	int i;
	struct stat buf;
	time_t mtime;
	char *path;
	char *deptree_cache, *depconfig;
	const char *service_dir = rc_svcdir();

	/* Create base directories if needed */
	for (i = 0; depdirs[i]; i++) {
		xasprintf(&path, "%s/%s", service_dir, depdirs[i]);
		if (mkdir(path, 0755) != 0 && errno != EEXIST)
			fprintf(stderr, "mkdir `%s': %s\n", depdirs[i], strerror(errno));
		free(path);
	}

	/* Quick test to see if anything we use has changed and we have
	 * data in our deptree. */

	xasprintf(&deptree_cache, "%s/deptree", service_dir);
	if (stat(deptree_cache, &buf) == 0) {
		mtime = buf.st_mtime;
	} else {
		/* No previous cache found.
		 * We still run the scan, in case of clock skew; we still need to return
		 * the newest time.
		 */
		newer = true;
		mtime = time(NULL);
	}
	free(deptree_cache);

	for (const char * const *dirs = rc_scriptdirs(); *dirs; dirs++) {
		static const char *subdirs[] = { "init.d", "conf.d", NULL };
		for (const char **subdir = subdirs; *subdir; subdir++) {
			xasprintf(&path, "%s/%s", *dirs, *subdir);
			newer |= !deep_mtime_check(path, true, &mtime, file);
			free(path);
		}
	}

	xasprintf(&path, "%s/rc.conf", rc_sysconfdir());
	newer |= !deep_mtime_check(path, true, &mtime, file);
	free(path);

	if (rc_is_user()) {
		xasprintf(&path, "%s/rc.conf", rc_usrconfdir());
		newer |= !deep_mtime_check(path, true, &mtime, file);
		free(path);
	}

	/* Some init scripts dependencies change depending on config files
	 * outside of baselayout, like syslog-ng, so we check those too. */
	xasprintf(&depconfig, "%s/depconfig", service_dir);
	config = rc_config_list(depconfig);
	TAILQ_FOREACH(s, config, entries) {
		newer |= !deep_mtime_check(s->value, true, &mtime, file);
	}
	rc_stringlist_free(config);
	free(depconfig);

	/* Return newest file time, if requested */
	if ((newer) && (newest != NULL)) {
	    *newest = mtime;
	}

	return newer;
}

static void
setup_environment(void) {
	char *scriptdirs, *env;
	size_t env_size = 0;
	struct utsname uts;

	for (const char * const *dirs = rc_scriptdirs(); *dirs; dirs++)
		env_size += strlen(*dirs) + sizeof(' ');

	env = scriptdirs = xmalloc(env_size);

	for (const char * const *dirs = rc_scriptdirs(); *dirs; dirs++) {
		int len = snprintf(scriptdirs, env_size, "%s ", *dirs);
		scriptdirs += len;
		env_size -= len;
	}

	setenv("RC_SCRIPTDIRS", env, 1);
	free(env);

	/* Some init scripts need RC_LIBEXECDIR to source stuff
	   Ideally we should be setting our full env instead */
	if (!getenv("RC_LIBEXECDIR"))
		setenv("RC_LIBEXECDIR", RC_LIBEXECDIR, 0);

	if (uname(&uts) == 0)
		setenv("RC_UNAME", uts.sysname, 1);
}

static void
rc_deplist_delete(struct rc_deplist *list, struct rc_depinfo *svc)
{
	struct rc_dep *dep;
	TAILQ_FOREACH(dep, list, entries)
		if (dep->service == svc) {
			TAILQ_REMOVE(list, dep, entries);
			free(dep);
			return;
		}
}

/* This is a 7 phase operation
   Phase 1 is a shell script which loads each init script and config in turn
   and echos their dependency info to stdout
   Phase 2 takes that and populates a depinfo object with that data
   Phase 3 adds any provided services to the depinfo object
   Phase 4 scans that depinfo object and puts in backlinks
   Phase 5 removes broken before dependencies
   Phase 6 looks for duplicate services indicating a real and virtual service
   with the same names
   Phase 7 saves the depinfo object to disk
   */
bool
rc_deptree_update(void)
{

	FILE *fp;
	RC_DEPTREE *deptree;
	RC_DEPINFO *depinfo = NULL, *depinfo_np, *di;
	struct rc_deplist *deptype = NULL, *dt, *provide;
	struct rc_dep *dep, *dep_np;
	RC_STRINGLIST *config, *dupes, *sorted;
	enum rc_deptype types;
	RC_STRING *s, *s3;
	char *line = NULL;
	size_t size;
	char *depend, *depends, *service, *type;
	char *deptree_cache, *depconfig;
	size_t i, l;
	bool retval = true;
	const char *sys = rc_sys();
	int serrno;

	/* Phase 1 - source all init scripts and print dependencies */
	setup_environment();
	if (!(fp = popen(GENDEP, "r")))
		return false;

	config = rc_stringlist_new();

	deptree = make_deptree();
	while (xgetline(&line, &size, fp) != -1) {
		depends = line;
		service = strsep(&depends, " ");
		if (!service || !*service)
			continue;

		type = strsep(&depends, " ");
		if (!depinfo || strcmp(depinfo->service, service) != 0) {
			deptype = NULL;
			if (!(depinfo = get_depinfo(deptree, service))) {
				depinfo = make_depinfo(deptree, service);
				depinfo->exists = true;
			}
		}

		/* We may not have any depends */
		if (!type || !depends)
			continue;

		/* Get the type */
		if (strcmp(type, "config") != 0) {
			int type_idx = rc_deptype_parse(type);
			if (type_idx == RC_DEPTYPE_INVALID)
				continue;
			deptype = &depinfo->depends[type_idx];
		}

		/* Now add each depend to our type.
		   We do this individually so we handle multiple spaces gracefully */
		while ((depend = strsep(&depends, " "))) {
			if (depend[0] == 0)
				continue;

			if (strcmp(type, "config") == 0) {
				rc_stringlist_addu(config, depend);
				continue;
			}

			/* Don't depend on ourself */
			if (strcmp(depend, service) == 0)
				continue;

			/* .sh files are not init scripts */
			l = strlen(depend);
			if (l > 2 &&
			    depend[l - 3] == '.' &&
			    depend[l - 2] == 's' &&
			    depend[l - 1] == 'h')
				continue;

			/* Remove our dependency if instructed */
			if (depend[0] == '!') {
				if ((di = get_depinfo(deptree, depend + 1)))
					rc_deplist_delete(deptype, di);
				continue;
			}

			if (!(di = get_depinfo(deptree, depend)))
				di = make_depinfo(deptree, depend);

			dep = xmalloc(sizeof(*dep));
			dep->service = di;
			TAILQ_INSERT_TAIL(deptype, dep, entries);

			/* We need to allow `after *; before local;` to work.
			 * Conversely, we need to allow 'before *; after modules' also */
			/* If we're before something, remove us from the after list */
			if (strcmp(type, "ibefore") == 0)
				rc_deplist_delete(&depinfo->depends[RC_DEPTYPE_IAFTER], di);
			/* If we're after something, remove us from the before list */
			if (strcmp(type, "iafter") == 0 || strcmp(type, "ineed") == 0
					|| strcmp(type, "iwant") == 0 || strcmp(type, "iuse") == 0) {
				rc_deplist_delete(&depinfo->depends[RC_DEPTYPE_IBEFORE], di);
			}
		}
	}
	free(line);
	pclose(fp);

	/* Phase 2 - if we're a special system, remove services that don't
	 * work for them. This doesn't stop them from being run directly. */
	if (sys) {
		char *nosys, *onosys;
		size_t len = strlen(sys);

		nosys = xmalloc(len + 2);
		nosys[0] = '-';
		for (i = 0; i < len; i++)
			nosys[i + 1] = (char)tolower((unsigned char)sys[i]);
		nosys[i + 1] = '\0';

		onosys = xmalloc(len + 3);
		onosys[0] = 'n';
		onosys[1] = 'o';
		for (i = 0; i < len; i++)
			onosys[i + 2] = (char)tolower((unsigned char)sys[i]);
		onosys[i + 2] = '\0';

		TAILQ_FOREACH_SAFE(depinfo, deptree, entries, depinfo_np) {
			deptype = &depinfo->depends[RC_DEPTYPE_KEYWORD];
			TAILQ_FOREACH(dep, deptype, entries) {
				const char *svcname = dep->service->service;
				if (strcmp(svcname, nosys) != 0 && strcmp(svcname, onosys) != 0)
					continue;
				provide = &depinfo->depends[RC_DEPTYPE_IPROVIDE];
				TAILQ_REMOVE(deptree, depinfo, entries);
				TAILQ_FOREACH(di, deptree, entries) {
					for (dt = di->depends; dt < di->depends + RC_DEPTYPE_MAX; dt++) {
						rc_deplist_delete(dt, depinfo);
						if (!TAILQ_EMPTY(provide)) {
							struct rc_dep *provided;
							TAILQ_FOREACH(provided, provide, entries)
								rc_deplist_delete(dt, provided->service);
						}
					}
				}
			}
		}
		free(nosys);
		free(onosys);
	}

	/* Phase 4 - backreference our depends */
	TAILQ_FOREACH(depinfo, deptree, entries) {
		for (i = 0; i < ARRAY_SIZE(deppairs); i++) {
			deptype = &depinfo->depends[deppairs[i].depend];
			TAILQ_FOREACH(dep, deptype, entries) {
				struct rc_dep *newdep = xmalloc(sizeof(*newdep));
				newdep->service = dep->service;
				if (!dep->service->exists) {
					if (deppairs[i].depend == RC_DEPTYPE_INEED) {
						fprintf(stderr, "Service '%s' needs non existent service '%s'\n",
							 depinfo->service, s->value);
						TAILQ_INSERT_TAIL(&depinfo->depends[RC_DEPTYPE_BROKEN], newdep, entries);
					}
					continue;
				}

				TAILQ_INSERT_TAIL(&depinfo->depends[deppairs[i].addto], newdep, entries);
			}
		}
	}

	/* Phase 5 - Remove broken before directives */
	types = RC_DEP(INEED) | RC_DEP(IWANT) | RC_DEP(IUSE) | RC_DEP(IAFTER);
	TAILQ_FOREACH(depinfo, deptree, entries) {
		deptype = &depinfo->depends[RC_DEPTYPE_IBEFORE];
		sorted = rc_stringlist_new();
		visit_service(deptree, types, sorted, depinfo, NULL, 0, 1);

		TAILQ_FOREACH_SAFE(dep, deptype, entries, dep_np) {
			TAILQ_FOREACH(s3, sorted, entries) {
				struct rc_dep *provided_dep = NULL;
				if (!(di = get_depinfo(deptree, s3->value)))
					continue;
				if (strcmp(dep->service->service, s3->value) == 0) {
					rc_deplist_delete(&di->depends[RC_DEPTYPE_IAFTER], depinfo);
					break;
				}
				TAILQ_FOREACH(provided_dep, &di->depends[RC_DEPTYPE_IPROVIDE], entries) {
					if (provided_dep->service == dep->service)
						break;
				}
				if (provided_dep) {
					rc_deplist_delete(&provided_dep->service->depends[RC_DEPTYPE_IAFTER], depinfo);
					break;
				}
			}
			if (s3)
				rc_deplist_delete(deptype, dep->service);
		}
		rc_stringlist_free(sorted);
	}

	/* Phase 6 - Print errors for duplicate services */
	dupes = rc_stringlist_new();
	TAILQ_FOREACH(depinfo, deptree, entries) {
		serrno = errno;
		errno = 0;
		rc_stringlist_addu(dupes,depinfo->service);
		if (errno == EEXIST) {
			fprintf(stderr,
					"Error: %s is the name of a real and virtual service.\n",
					depinfo->service);
		}
		errno = serrno;
	}
	rc_stringlist_free(dupes);

	/* Phase 7 - save to disk
	   Now that we're purely in C, do we need to keep a shell parseable file?
	   I think yes as then it stays human readable
	   This works and should be entirely shell parseable provided that depend
	   names don't have any non shell variable characters in
	   */
	xasprintf(&deptree_cache, "%s/deptree", rc_svcdir());
	if ((fp = fopen(deptree_cache, "w"))) {
		i = 0;
		TAILQ_FOREACH(depinfo, deptree, entries) {
			fprintf(fp, "depinfo_%zu_service='%s'\n", i, depinfo->service);
			for (size_t d = 0; d < RC_DEPTYPE_MAX; d++) {
				size_t k = 0;
				TAILQ_FOREACH(dep, &depinfo->depends[d], entries) {
					fprintf(fp, "depinfo_%zu_%s_%zu='%s'\n",
							i, deptype_map[d], k++, dep->service->service);
				}
			}
			i++;
		}
		fclose(fp);
	} else {
		fprintf(stderr, "fopen '%s': %s\n", deptree_cache, strerror(errno));
		retval = false;
	}
	free(deptree_cache);

	/* Save our external config files to disk */
	xasprintf(&depconfig, "%s/depconfig", rc_svcdir());
	if (TAILQ_FIRST(config)) {
		if ((fp = fopen(depconfig, "w"))) {
			TAILQ_FOREACH(s, config, entries)
				fprintf(fp, "%s\n", s->value);
			fclose(fp);
		} else {
			fprintf(stderr, "fopen '%s': %s\n", depconfig, strerror(errno));
			retval = false;
		}
	} else {
		unlink(depconfig);
	}
	free(depconfig);

	rc_stringlist_free(config);
	rc_deptree_free(deptree);
	return retval;
}
