/*
   librc-depend
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

#include <sys/utsname.h>
#include <search.h>		/* hsearch() */
#include <stdint.h>		/* uint32_t/uint64_t */

#include "librc.h"
#include "../include/einfo.h"

#define GENDEP			RC_LIBEXECDIR "/sh/gendepends.sh"

#define RC_DEPCONFIG		RC_SVCDIR "/depconfig"

#define LOOPSOLVER_LIMIT	128

/*! Type definition of service ID */
typedef uint32_t service_id_t;

/*! Enumeration of rc_deptree_solve_loop()'s return cases */
typedef enum loopfound {
	LOOP_SOLVABLE	= 0x01,
	LOOP_UNSOLVABLE	= 0x02,
} loopfound_t;

/* "use, need, after" dependencies matrix types */
typedef enum unapm_type {
	UNAPM_USE            = 0,
	UNAPM_AFTER          = 1,
	UNAPM_NEED           = 2,
	UNAPM_PROVIDEDBY     = 3,
	UNAPM_MIXED          = 4,
	UNAPM_MIXED_EXPANDED = 5,
	UNAPM_MAX
} unapm_type_t;

typedef struct idid_entry {
	uint64_t	idid;
	void		*data;
} idid_entry_t;

static const char *bootlevel = NULL;

static char *
get_shell_value(char *string)
{
	char *p = string;
	char *e;

	if (! string)
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
	RC_DEPINFO *di2;
	RC_DEPTYPE *dt;
	RC_DEPTYPE *dt2;

	if (!deptree)
		return;

	di = TAILQ_FIRST(deptree);
	while (di) {
		di2 = TAILQ_NEXT(di, entries);
		dt = TAILQ_FIRST(&di->depends);
		while (dt) {
			dt2 = TAILQ_NEXT(dt, entries);
			rc_stringlist_free(dt->services);
			free(dt->type);
			free(dt);
			dt = dt2;
		}
		free(di->service);
		free(di);
		di = di2;
	}
	free(deptree);
}
librc_hidden_def(rc_deptree_free)

static RC_DEPINFO *
get_depinfo(const RC_DEPTREE *deptree, const char *service)
{
	RC_DEPINFO *di;

	TAILQ_FOREACH(di, deptree, entries)
		if (strcmp(di->service, service) == 0)
			return di;
	return NULL;
}

static RC_DEPTYPE *
get_deptype(const RC_DEPINFO *depinfo, const char *type)
{
	RC_DEPTYPE *dt;

	TAILQ_FOREACH(dt, &depinfo->depends, entries)
		if (strcmp(dt->type, type) == 0)
			return dt;
	return NULL;
}

RC_DEPTREE *
rc_deptree_load(void) {
	return rc_deptree_load_file(RC_DEPTREE_CACHE);
}
librc_hidden_def(rc_deptree_load)

RC_DEPTREE *
rc_deptree_load_file(const char *deptree_file)
{
	FILE *fp;
	RC_DEPTREE *deptree;
	RC_DEPINFO *depinfo = NULL;
	RC_DEPTYPE *deptype = NULL;
	char *line = NULL;
	size_t len = 0;
	char *type;
	char *p;
	char *e;
	int i;

	if (!(fp = fopen(deptree_file, "r")))
		return NULL;

	deptree = xmalloc(sizeof(*deptree));
	TAILQ_INIT(deptree);
	while ((rc_getline(&line, &len, fp)))
	{
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
			if (! e || *e == '\0')
				continue;
			depinfo = xmalloc(sizeof(*depinfo));
			TAILQ_INIT(&depinfo->depends);
			depinfo->service = xstrdup(e);
			TAILQ_INSERT_TAIL(deptree, depinfo, entries);
			deptype = NULL;
			continue;
		}
		e = strsep(&p, "=");
		if (!e || sscanf(e, "%d", &i) != 1)
			continue;
		/* Sanity */
		e = get_shell_value(p);
		if (!e || *e == '\0')
			continue;
		if (!deptype || strcmp(deptype->type, type) != 0) {
			deptype = xmalloc(sizeof(*deptype));
			deptype->services = rc_stringlist_new();
			deptype->type = xstrdup(type);
			TAILQ_INSERT_TAIL(&depinfo->depends, deptype, entries);
		}
		rc_stringlist_add(deptype->services, e);
	}
	fclose(fp);
	free(line);

	return deptree;
}
librc_hidden_def(rc_deptree_load_file)

static bool
valid_service(const char *runlevel, const char *service, const char *type)
{
	RC_SERVICE state;

	if (!runlevel ||
	    strcmp(type, "ineed") == 0 ||
	    strcmp(type, "needsme") == 0)
		return true;

	if (rc_service_in_runlevel(service, runlevel))
		return true;
	if (strcmp(runlevel, RC_LEVEL_SYSINIT) == 0)
		    return false;
	if (strcmp(runlevel, RC_LEVEL_SHUTDOWN) == 0 &&
	    strcmp(type, "iafter") == 0)
		    return false;
	if (strcmp(runlevel, bootlevel) != 0) {
		if (rc_service_in_runlevel(service, bootlevel))
			return true;
	}

	state = rc_service_state(service);
	if (state & RC_SERVICE_HOTPLUGGED ||
	    state & RC_SERVICE_STARTED)
		return true;

	return false;
}

static bool
get_provided1(const char *runlevel, RC_STRINGLIST *providers,
	      RC_DEPTYPE *deptype, const char *level,
	      bool hotplugged, RC_SERVICE state)
{
	RC_STRING *service;
	RC_SERVICE st;
	bool retval = false;
	bool ok;
	const char *svc;

	TAILQ_FOREACH(service, deptype->services, entries) {
		ok = true;
		svc = service->value;
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
	RC_DEPTYPE *dt;
	RC_STRINGLIST *providers = rc_stringlist_new();
	RC_STRING *service;

	dt = get_deptype(depinfo, "providedby");
	if (!dt)
		return providers;

	/* If we are stopping then all depends are true, regardless of state.
	   This is especially true for net services as they could force a restart
	   of the local dns resolver which may depend on net. */
	if (options & RC_DEP_STOP) {
		TAILQ_FOREACH(service, dt->services, entries)
			rc_stringlist_add(providers, service->value);
		return providers;
	}

	/* If we're strict or starting, then only use what we have in our
	 * runlevel and bootlevel. If we starting then check hotplugged too. */
	if (options & RC_DEP_STRICT || options & RC_DEP_START) {
		TAILQ_FOREACH(service, dt->services, entries)
			if (rc_service_in_runlevel(service->value, runlevel) ||
			    rc_service_in_runlevel(service->value, bootlevel) ||
			    (options & RC_DEP_START &&
			     rc_service_state(service->value) & RC_SERVICE_HOTPLUGGED))
				rc_stringlist_add(providers, service->value);
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
	if (get_provided1(runlevel, providers, dt, runlevel, false, RC_SERVICE_STARTED))
	{ DO }
	if (get_provided1(runlevel, providers, dt, NULL, true, RC_SERVICE_STARTED))
	{ DO }
	if (bootlevel && strcmp(runlevel, bootlevel) != 0 &&
	    get_provided1(runlevel, providers, dt, bootlevel, false, RC_SERVICE_STARTED))
	{ DO }
	if (get_provided1(runlevel, providers, dt, NULL, false, RC_SERVICE_STARTED))
	{ DO }

	/* Check starting services */
	if (get_provided1(runlevel, providers, dt, runlevel, false, RC_SERVICE_STARTING))
		return providers;
	if (get_provided1(runlevel, providers, dt, NULL, true, RC_SERVICE_STARTING))
		return providers;
	if (bootlevel && strcmp(runlevel, bootlevel) != 0 &&
	    get_provided1(runlevel, providers, dt, bootlevel, false, RC_SERVICE_STARTING))
	    return providers;
	if (get_provided1(runlevel, providers, dt, NULL, false, RC_SERVICE_STARTING))
		return providers;

	/* Nothing started then. OK, lets get the stopped services */
	if (get_provided1(runlevel, providers, dt, runlevel, false, RC_SERVICE_STOPPED))
		return providers;
	if (get_provided1(runlevel, providers, dt, NULL, true, RC_SERVICE_STOPPED))
	{ DO }
	if (bootlevel && (strcmp(runlevel, bootlevel) != 0) &&
	    get_provided1(runlevel, providers, dt, bootlevel, false, RC_SERVICE_STOPPED))
		return providers;

	/* Still nothing? OK, list our first provided service. */
	service = TAILQ_FIRST(dt->services);
	if (service != NULL)
		rc_stringlist_add(providers, service->value);

	return providers;
}

static void
visit_service(const RC_DEPTREE *deptree,
	      const RC_STRINGLIST *types,
	      RC_STRINGLIST *sorted,
	      RC_STRINGLIST *visited,
	      const RC_DEPINFO *depinfo,
	      const char *runlevel, int options)
{
	RC_STRING *type;
	RC_STRING *service;
	RC_DEPTYPE *dt;
	RC_DEPINFO *di;
	RC_STRINGLIST *provided;
	RC_STRING *p;
	const char *svcname;

	/* Check if we have already visited this service or not */
	TAILQ_FOREACH(type, visited, entries)
		if (strcmp(type->value, depinfo->service) == 0)
			return;
	/* Add ourselves as a visited service */
	rc_stringlist_add(visited, depinfo->service);

	TAILQ_FOREACH(type, types, entries)
	{
		if (!(dt = get_deptype(depinfo, type->value)))
			continue;

		TAILQ_FOREACH(service, dt->services, entries) {
			if (!(options & RC_DEP_TRACE) ||
			    strcmp(type->value, "iprovide") == 0)
			{
				rc_stringlist_add(sorted, service->value);
				continue;
			}

			if (!(di = get_depinfo(deptree, service->value)))
				continue;
			provided = get_provided(di, runlevel, options);

			if (TAILQ_FIRST(provided)) {
				TAILQ_FOREACH(p, provided, entries) {
					di = get_depinfo(deptree, p->value);
					if (di && valid_service(runlevel, di->service, type->value))
						visit_service(deptree, types, sorted, visited, di,
							      runlevel, options | RC_DEP_TRACE);
				}
			}
			else if (di && valid_service(runlevel, service->value, type->value))
				visit_service(deptree, types, sorted, visited, di,
					      runlevel, options | RC_DEP_TRACE);

			rc_stringlist_free(provided);
		}
	}

	/* Now visit the stuff we provide for */
	if (options & RC_DEP_TRACE &&
	    (dt = get_deptype(depinfo, "iprovide")))
	{
		TAILQ_FOREACH(service, dt->services, entries) {
			if (!(di = get_depinfo(deptree, service->value)))
				continue;
			provided = get_provided(di, runlevel, options);
			TAILQ_FOREACH(p, provided, entries)
				if (strcmp(p->value, depinfo->service) == 0) {
					visit_service(deptree, types, sorted, visited, di,
						       runlevel, options | RC_DEP_TRACE);
					break;
				}
			rc_stringlist_free(provided);
		}
	}

	/* We've visited everything we need, so add ourselves unless we
	   are also the service calling us or we are provided by something */
	svcname = getenv("RC_SVCNAME");
	if (!svcname || strcmp(svcname, depinfo->service) != 0) {
		if (!get_deptype(depinfo, "providedby"))
			rc_stringlist_add(sorted, depinfo->service);
	}
}

RC_STRINGLIST *
rc_deptree_depend(const RC_DEPTREE *deptree,
		  const char *service, const char *type)
{
	RC_DEPINFO *di;
	RC_DEPTYPE *dt;
	RC_STRINGLIST *svcs;
	RC_STRING *svc;

	svcs = rc_stringlist_new();
	if (!(di = get_depinfo(deptree, service)) ||
	    !(dt = get_deptype(di, type)))
	{
		errno = ENOENT;
		return svcs;
	}

	/* For consistency, we copy the array */
	TAILQ_FOREACH(svc, dt->services, entries)
		rc_stringlist_add(svcs, svc->value);
	return svcs;
}
librc_hidden_def(rc_deptree_depend)

RC_STRINGLIST *
rc_deptree_depends(const RC_DEPTREE *deptree,
		   const RC_STRINGLIST *types,
		   const RC_STRINGLIST *services,
		   const char *runlevel, int options)
{
	RC_STRINGLIST *sorted = rc_stringlist_new();
	RC_STRINGLIST *visited = rc_stringlist_new();
	RC_DEPINFO *di;
	const RC_STRING *service;

	bootlevel = getenv("RC_BOOTLEVEL");
	if (!bootlevel)
		bootlevel = RC_LEVEL_BOOT;
	TAILQ_FOREACH(service, services, entries) {
		if (!(di = get_depinfo(deptree, service->value))) {
			errno = ENOENT;
			continue;
		}
		if (types)
			visit_service(deptree, types, sorted, visited,
				      di, runlevel, options);
	}
	rc_stringlist_free(visited);
	return sorted;
}
librc_hidden_def(rc_deptree_depends)

RC_STRINGLIST *
rc_deptree_order(const RC_DEPTREE *deptree, const char *runlevel, int options)
{
	RC_STRINGLIST *list;
	RC_STRINGLIST *list2;
	RC_STRINGLIST *types;
	RC_STRINGLIST *services;

	bootlevel = getenv("RC_BOOTLEVEL");
	if (! bootlevel)
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
	types = rc_stringlist_new();
	rc_stringlist_add(types, "ineed");
	rc_stringlist_add(types, "iuse");
	rc_stringlist_add(types, "iafter");
	services = rc_deptree_depends(deptree, types, list, runlevel,
				      RC_DEP_STRICT | RC_DEP_TRACE | options);
	rc_stringlist_free(list);
	rc_stringlist_free(types);
	return services;
}
librc_hidden_def(rc_deptree_order)

static bool
mtime_check(const char *source, const char *target, bool newer,
	    time_t *rel, char *file)
{
	struct stat buf;
	time_t mtime;
	bool retval = true;
	DIR *dp;
	struct dirent *d;
	char path[PATH_MAX];
	int serrno = errno;

	/* We have to exist */
	if (stat(source, &buf) != 0)
		return false;
	mtime = buf.st_mtime;

	/* If target does not exist, return true to mimic shell test */
	if (stat(target, &buf) != 0)
		return true;

	if (newer) {
		if (mtime < buf.st_mtime) {
			if (rel == NULL)
				return false;
			retval = false;
		}
		if (rel != NULL) {
			if (*rel < buf.st_mtime) {
				if (file)
					strlcpy(file, target, PATH_MAX);
				*rel = buf.st_mtime;
			}
		}
	} else {
		if (mtime > buf.st_mtime) {
			if (rel == NULL)
				return false;
			retval = false;
		}
		if (rel != NULL) {
			if (*rel > buf.st_mtime) {
				if (file)
					strlcpy(file, target, PATH_MAX);
				*rel = buf.st_mtime;
			}
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
		if (!mtime_check(source, path, newer, rel, file)) {
			retval = false;
			if (rel == NULL)
				break;
		}
	}
	closedir(dp);
	return retval;
}

bool
rc_newer_than(const char *source, const char *target,
	      time_t *newest, char *file)
{

	return mtime_check(source, target, true, newest, file);
}
librc_hidden_def(rc_newer_than)

bool
rc_older_than(const char *source, const char *target,
	      time_t *oldest, char *file)
{
	return mtime_check(source, target, false, oldest, file);
}
librc_hidden_def(rc_older_than)

typedef struct deppair
{
	const char *depend;
	const char *addto;
} DEPPAIR;

static const DEPPAIR deppairs[] = {
	{ "ineed",	"needsme" },
	{ "iuse",	"usesme" },
	{ "iafter",	"ibefore" },
	{ "ibefore",	"iafter" },
	{ "iprovide",	"providedby" },
	{ NULL, NULL }
};

static const char *const depdirs[] =
{
	RC_SVCDIR,
	RC_SVCDIR "/starting",
	RC_SVCDIR "/started",
	RC_SVCDIR "/stopping",
	RC_SVCDIR "/inactive",
	RC_SVCDIR "/wasinactive",
	RC_SVCDIR "/failed",
	RC_SVCDIR "/hotplugged",
	RC_SVCDIR "/daemons",
	RC_SVCDIR "/options",
	RC_SVCDIR "/exclusive",
	RC_SVCDIR "/scheduled",
	RC_SVCDIR "/tmp",
	NULL
};

bool
rc_deptree_update_needed(time_t *newest, char *file)
{
	bool newer = false;
	RC_STRINGLIST *config;
	RC_STRING *s;
	int i;

	/* Create base directories if needed */
	for (i = 0; depdirs[i]; i++)
		if (mkdir(depdirs[i], 0755) != 0 && errno != EEXIST)
			fprintf(stderr, "mkdir `%s': %s\n", depdirs[i], strerror(errno));

	/* Quick test to see if anything we use has changed and we have
	 * data in our deptree */
	if (!existss(RC_DEPTREE_CACHE))
		return true;
	if (!rc_newer_than(RC_DEPTREE_CACHE, RC_INITDIR, newest, file))
		return true;
	if (!rc_newer_than(RC_DEPTREE_CACHE, RC_CONFDIR, newest, file))
		return true;
#ifdef RC_PKG_INITDIR
	if (!rc_newer_than(RC_DEPTREE_CACHE, RC_PKG_INITDIR, newest, file))
		return true;
#endif
#ifdef RC_PKG_CONFDIR
	if (!rc_newer_than(RC_DEPTREE_CACHE, RC_PKG_CONFDIR, newest, file))
		return true;
#endif
#ifdef RC_LOCAL_INITDIR
	if (!rc_newer_than(RC_DEPTREE_CACHE, RC_LOCAL_INITDIR, newest, file))
		return true;
#endif
#ifdef RC_LOCAL_CONFDIR
	if (!rc_newer_than(RC_DEPTREE_CACHE, RC_LOCAL_CONFDIR, newest, file))
		return true;
#endif
	if (!rc_newer_than(RC_DEPTREE_CACHE, RC_CONF, newest, file))
		return true;

	/* Some init scripts dependencies change depending on config files
	 * outside of baselayout, like syslog-ng, so we check those too. */
	config = rc_config_list(RC_DEPCONFIG);
	TAILQ_FOREACH(s, config, entries) {
		if (!rc_newer_than(RC_DEPTREE_CACHE, s->value, newest, file)) {
			newer = true;
			break;
		}
	}
	rc_stringlist_free(config);
	return newer;
}
librc_hidden_def(rc_deptree_update_needed)

static inline int
rc_deptree_unapm_expandsdeps(service_id_t **unap, service_id_t service_id)
{
	int dep_num, dep_count;
	int ismodified;

	ismodified = 0;
	dep_num    = 0;
	dep_count  = unap[service_id][0];
	while (dep_num < dep_count) {
		service_id_t dep_service_id;
		int dep_dep_num, dep_dep_count;

		dep_num++;
		dep_service_id = unap[service_id][dep_num];
		/*printf("service_id == %i; dep_num == %i (%i); dep_service_id == %i\n", service_id, dep_num, dep_count, dep_service_id);*/

		dep_dep_num   = 0;
		dep_dep_count = unap[dep_service_id][0];

		while (dep_dep_num < dep_dep_count) {
			int istoadd, dep_num_2;
			service_id_t dep_dep_service_id;
			dep_dep_num++;

			dep_dep_service_id = unap[dep_service_id][dep_dep_num];

			istoadd   = 1;
			dep_num_2 = 0;
			while (dep_num_2 < dep_count) {
				dep_num_2++;
				if (dep_dep_service_id == unap[service_id][dep_num_2]) {
					istoadd = 0;
					break;
				}
			}

			if (istoadd) {
				ismodified = 1;
				dep_count++;
				unap[service_id][dep_count] = dep_dep_service_id;
				unap[service_id][0]         = dep_count;
			}
		}
	}

	return ismodified;
}

/*! Fills dependency matrix for further loop detection
 * @param unap_matrix matrix to fill
 * @param useneedafter_count number of use/need/after/provide dependencies (zero to do not allocate memory)
 * @param service_id ID of the service for dependency scanning
 * @param type dependencies type
 * @param depinfo dependencies information */
static void
rc_deptree_unapm_getdependencies(service_id_t **unap_matrix,
	int useneedafter_count, service_id_t service_id,
	const char *type, RC_DEPINFO *depinfo)
{
	RC_STRING *svc, *svc_np;
	RC_DEPTYPE *deptype;

	if (useneedafter_count)
		unap_matrix[service_id] = xcalloc((useneedafter_count+1), sizeof(**unap_matrix));

	deptype = get_deptype(depinfo, type);
	if (deptype == NULL)
		return;

	TAILQ_FOREACH_SAFE(svc, deptype->services, entries, svc_np) {
		ENTRY item, *item_p;
		service_id_t dependon;

		item.key = svc->value;

		item_p = hsearch(item, FIND);
		if (item_p == NULL)	/* Deadend branch, no sense to continue checking it anyway */
			continue;

		dependon = (int)(long int)item_p->data;

		if (dependon == service_id)
			continue;	/* To prevent looping detection services on themselves (for example in case of depending on '*') */

		unap_matrix[service_id][ ++unap_matrix[service_id][0] ] = dependon;
	}

	return;
}

static int
svc_id2depinfo_bt_compare(const void *a, const void *b)
{
	return ((const ENTRY *)a)->key - ((const ENTRY *)b)->key;
}

static int
idid_compare(const void *a, const void *b)
{
	return ((const idid_entry_t *)a)->idid - ((const idid_entry_t *)b)->idid;
}

static int
idid_compare_data_desc(const void *a, const void *b)
{
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpointer-arith"
	return ((const idid_entry_t *)b)->data - ((const idid_entry_t *)a)->data;
#pragma GCC diagnostic pop
}

static inline char
deptype2char(unapm_type_t type) {
	switch (type) {
		case UNAPM_USE:
			return 'u';
		case UNAPM_AFTER:
			return 'a';
		case UNAPM_NEED:
			return 'n';
		case UNAPM_PROVIDEDBY:
			return 'p';
		default:	/* shouldn't happend */
			return '?';
	}

	return '?';
}

/*! Solves dependecies loops
 * @param unap_matrix matrixes to scan ways to solve the loop
 * @param service_id looped service id
 * @param svc_id2depinfo_bt ptr to binary tree root to get depinfo by svc id
 * @param end_dep_num looping dependency id in use/need/after/provide matrix line */
static loopfound_t
rc_deptree_solve_loop(service_id_t **unap_matrix[UNAPM_MAX], service_id_t service_id, void *svc_id2depinfo_bt, int end_dep_num) {
	char **chain_strs = NULL;
	service_id_t **chains;
	unapm_type_t **deptypes;
	unapm_type_t minimal_cost;
	int chains_size = unap_matrix[0][0][0], chain_count;

	chains = xmalloc(chains_size * sizeof(*chains));

	/* building all dependency chains of the service */

	{

		int dep_num, unap_line_size;
		service_id_t *unap_line;

		unap_line_size = unap_matrix[UNAPM_MIXED_EXPANDED][service_id][0]+1;
		unap_line =  xmalloc(unap_line_size * sizeof(*unap_line));

		memcpy(unap_line, unap_matrix[UNAPM_MIXED_EXPANDED][service_id], unap_line_size * sizeof(*unap_line));

		unap_line[0] =  service_id;
		chain_count =  0;
		dep_num     = -1;
		while (++dep_num < end_dep_num) {
			int dep_dep_num, added_count;
			service_id_t dep_service_id;

			dep_service_id = unap_line[dep_num];
			dep_dep_num    = dep_num;
			added_count    = 0;
			while (++dep_dep_num <= end_dep_num) {
				int chain_num, chain_count_new, dep_dep_check_num, dep_dep_check_count, istobeadded;
				service_id_t dep_dep_service_id, dep_dep_service_id_added;

				dep_dep_service_id = unap_line[dep_dep_num];

				dep_dep_check_num   = 0;
				dep_dep_check_count = unap_matrix[UNAPM_MIXED][dep_service_id][0];
				istobeadded         = 0;
				while (dep_dep_check_num++ < dep_dep_check_count) {
					if (unap_matrix[UNAPM_MIXED][dep_service_id][dep_dep_check_num] == dep_dep_service_id) {
						istobeadded = 1;
						break;
					}
				}

				if (istobeadded) {
					int chain_len;

					chain_num       = 0;
					chain_count_new = chain_count;

#define					CHAINS_CHECK_SIZE {\
						if (chain_count_new >= chains_size) {\
							chains_size += unap_matrix[0][0][0];\
							chains       = xrealloc(chains, chains_size*sizeof(*chains));\
						}\
						/*printf("A: %i\n", chain_count_new);*/\
						chains[chain_count_new]    = xmalloc(end_dep_num*sizeof(**chains));\
						chains[chain_count_new][0] = 0;\
					}

					if (dep_num == 0) {
						CHAINS_CHECK_SIZE;
						chains[chain_count_new][1] = dep_dep_service_id;
						chains[chain_count_new][0] = 1;
						chain_count_new++;
					} else
					while (chain_num < chain_count) {
						if (!added_count) {
							chain_len = chains[chain_num][0];
							if (chains[chain_num][chain_len] == dep_service_id) {
								chains[chain_num][++chain_len] = dep_dep_service_id;
								dep_dep_service_id_added       = dep_dep_service_id;
								chains[chain_num][0]           = chain_len;
							}
						} else {
							/* required chains were been enlarged by previous iteration, so "-1" */
							chain_len = chains[chain_num][0]-1;
							if (chains[chain_num][chain_len] == dep_service_id && chains[chain_num][chain_len+1] == dep_dep_service_id_added) {
								CHAINS_CHECK_SIZE;
								memcpy(chains[chain_count_new], chains[chain_num], (chain_len+1)*sizeof(**chains));
								chains[chain_count_new][++chain_len] = dep_dep_service_id;
								chains[chain_count_new][0]           = chain_len;
								chain_count_new++;
							}
						}
						chain_num++;
					}
					added_count++;
					chain_count = chain_count_new;
				}
			}
		}

		free(unap_line);
	}

	/* removing non-looping chains */

	{
		int i;

		i = 0;
		while (i < chain_count) {
			int j, chain_len, islooping;

			chain_len = chains[i][0];

			islooping = 0;
			j         = 0;
			while (j++ < chain_len)
				if (chains[i][j] == service_id) {
					islooping = 1;
					break;
				}

			if (!islooping) {
				free(chains[i]);
				chains[i] = chains[--chain_count];
				continue;
			}

			i++;
		}
	}

	/* getting dependencies types */

	{
		int i;
		deptypes = xmalloc(chain_count * sizeof(*deptypes));

		i = 0;
		while (i < chain_count) {
			int j, chain_len;

			chain_len = chains[i][0];

			deptypes[i]    = xmalloc((chain_len+1) * sizeof(**deptypes));

			j     = 0;
			while (j++ < chain_len) {
				service_id_t dep_service_id_from, dep_service_id_to;
				unapm_type_t type;

				dep_service_id_from = j==1 ? service_id : chains[i][j-1];
				dep_service_id_to   =                     chains[i][j];

				type = UNAPM_PROVIDEDBY+1;
				while (type-- > 0) {
					int dep_dep_num, dep_dep_count;
					dep_dep_num   = 0;
					dep_dep_count = unap_matrix[type][dep_service_id_from][0];

					while (dep_dep_num++ < dep_dep_count) {
						if (unap_matrix[type][dep_service_id_from][dep_dep_num] == dep_service_id_to) {
							deptypes[i][j] 	= type;
							type 		= 0;	/* to break parent while (), too */
							break;
						}
					}
				}
			}
			i++;
		}
	}

	/* printing */

	if (svc_id2depinfo_bt != NULL) {
		int chain_num;
		chain_strs = xmalloc(chain_count * sizeof(*chain_strs));

		chain_num = 0;
		while (chain_num < chain_count) {
			char *chain_str, *chain_str_end;
			int chain_len;

			chain_str     = chain_strs[chain_num] = xmalloc(BUFSIZ);
			chain_str_end = &chain_str[BUFSIZ-2];

			/* Preparing a string of services forming the loop */
			{
				char *ptr_dst;
				int i;

				ptr_dst = chain_str;

				chain_len = chains[chain_num][0];
				chains[chain_num][0] = service_id;

				i = chain_len+1;
				while (i--) {
					ENTRY item, **item_pp;
					RC_DEPINFO *depinfo;
					const char *service_name, *ptr_src;

					item.key = (void *)(long)chains[chain_num][i];
					item_pp  = tfind(&item, &svc_id2depinfo_bt, svc_id2depinfo_bt_compare);
					depinfo  = (RC_DEPINFO *)((ENTRY *)*item_pp)->data;

					service_name = depinfo->service;

					ptr_src = service_name;
					while (*ptr_src && (ptr_dst < chain_str_end))
						*(ptr_dst++) = *(ptr_src++);

					if (ptr_dst >= chain_str_end) {
						ptr_dst--;
						break;
					}

					if (i) {
						if (&ptr_dst[4] >= chain_str_end)
							break;

						*(ptr_dst++) = ' ';
						*(ptr_dst++) = deptype2char(deptypes[chain_num][i]);
						memcpy(ptr_dst, "> ", 2);
						ptr_dst += 2;
					}
				}

				chains[chain_num][0] = chain_len;

				*ptr_dst = 0;
			}

/*
			{
				int j;
				chain_len = chains[chain_num][0];
				j = 0;
				printf("%i: %i (%i):", service_id, chain_num, chain_len);
				while (j++ < chain_len)
					printf(" %i<%i>", chains[chain_num][j], deptypes[chain_num][j]);
				printf("\n");
			}
*/
			chain_num++;
		}

	}

	/* checking the cost of loop solving (use/after/need) */

	{
		int i;

		minimal_cost = 0;
		i = 0;
		while (i < chain_count) {
			int j, chain_len;
			unapm_type_t chain_cost;

			chain_cost = UNAPM_MAX;

			chain_len = chains[i][0];
			j         = 0;
			while (j++ < chain_len)
				chain_cost = MIN(chain_cost, deptypes[i][j]);

			minimal_cost = MAX(minimal_cost, chain_cost);

			if (svc_id2depinfo_bt != NULL) {
/*
				RC_DEPINFO *depinfo;
				ENTRY item, **item_pp;
				item.key = (void *)(long)service_id;
				item_pp  = tfind(&item, &svc_id2depinfo_bt, svc_id2depinfo_bt_compare);
				depinfo  = (RC_DEPINFO *)((ENTRY *)*item_pp)->data;
*/
				if (minimal_cost > UNAPM_AFTER)
					eerror("Found an unresolvable dependency: %s.", chain_strs[i]);
				else
					ewarn("Found a solvable dependency loop: %s.", chain_strs[i]);
			}
			i++;
		}
		/*printf("minimal cost: %i\n", minimal_cost);*/

		if (minimal_cost > UNAPM_AFTER)
			return LOOP_UNSOLVABLE;
	}

	/* calculating optimal way to solve the loop and solving it */

	{
		void *btree = NULL;
		uint64_t *idid_to_break;
		int idid_count, idid_to_break_count;
		idid_to_break = xmalloc(chain_count * sizeof(*idid_to_break));

		/* counting a presence of each dependency through all chains */
		{
			int i;

			idid_count = 0;
			i = 0;
			while (i < chain_count) {
				int j, chain_len;

				chain_len = chains[i][0];
				j         = 0;
				while (j++ < chain_len) {
					uint64_t idid;
					service_id_t service_id_from, service_id_to;
					idid_entry_t idid_entry, *idid_entry_p;
					void *tfind_res;

					if (deptypes[i][j] > minimal_cost)	/* we don't break this dependency, skipping */
						continue;

					service_id_from = j>1 ? chains[i][j-1] : service_id;
					service_id_to   =       chains[i][j];

					idid  =  ((uint64_t)service_id_from << bitsizeof(service_id_to)) | service_id_to;

					idid_entry.idid = idid;
					tfind_res       = tfind(&idid_entry, &btree, idid_compare);
					/*printf("A: (%i -> %i) %li: %p: %p\n", service_id_from, service_id_to, idid, tfind_res, tfind_res==NULL?NULL:*(idid_entry_t **)tfind_res);*/
					if (tfind_res == NULL) {
						idid_entry_p       = xmalloc(sizeof(*idid_entry_p));
						idid_entry_p->idid = idid;
						idid_entry_p->data = (void *)1;
						tsearch(idid_entry_p, &btree, idid_compare);
						idid_count++;
					} else {
						idid_entry_p       = *(idid_entry_t **)tfind_res;
						idid_entry_p->data = (void *)((long)idid_entry_p->data + 1);
					}
				}

				i++;
			}
		}

		/* building array of dependencies sorted by descending presence counter */

		{
			int idid_count2;
			idid_entry_t *idid_counters;

			void idid_btree_builddescarray(const void *nodep, const VISIT which, const int depth) {
				(void)depth;
				switch (which) {
					case preorder:
					case leaf: {
						const idid_entry_t *idid_entry_p = *(idid_entry_t * const*)nodep;

						memcpy(&idid_counters[idid_count2], idid_entry_p, sizeof(idid_counters[idid_count2]));
						idid_count2++;
						break;
					}
					default:
						break;
				}
				return;
			}

			idid_counters = xmalloc(idid_count * sizeof(*idid_counters));

			idid_count2 = 0;

			twalk(btree, idid_btree_builddescarray);

			qsort(idid_counters, idid_count2, sizeof(*idid_counters), idid_compare_data_desc);

			idid_to_break_count = MIN(idid_count2, chain_count);
			idid_count2 = 0;
			while (idid_count2 < idid_to_break_count) {
				idid_to_break[idid_count2] = idid_counters[idid_count2].idid;
				/*printf("B: %li %li\n", idid_counters[idid_count2].idid, (long)idid_counters[idid_count2].data);*/
				idid_count2++;
			}

			free(idid_counters);
		}

		/* solving loops */

		{
			idid_count = 0;
			while (idid_count < idid_to_break_count) {
				service_id_t service_id_from, service_id_to;

				service_id_from = ((uint64_t)idid_to_break[idid_count] & ((uint64_t)((service_id_t)~0) << bitsizeof(service_id_t))) >> bitsizeof(service_id_t);
				service_id_to   =            idid_to_break[idid_count] &            ((service_id_t)~0);

				idid_count++;

				{
					ENTRY item, **item_pp;
					RC_DEPINFO *depinfo_from = NULL, *depinfo_to = NULL;

					void
					rc_deptree_remove_loopdependency(service_id_t **unap[UNAPM_MAX], service_id_t dep_remove_from_service_id, service_id_t dep_remove_to_service_id, RC_DEPINFO *di_from, RC_DEPINFO *di_to, const char *const type, unapm_type_t unapm_type)
					{
						RC_DEPTYPE *deptype_from = NULL, *deptype_to = NULL;
						int dep_num, dep_count;
						const char *type_reverse = NULL;
						int deptype_num;

						/* removing use/after from cache */
						if (di_from != NULL) {
							deptype_from = get_deptype(di_from, type);
							if (deptype_from != NULL)
								rc_stringlist_delete(deptype_from->services, di_to->service);
						}

						if (di_to != NULL)
							deptype_to = get_deptype(di_to, type_reverse);

						/* removing from the UNAP matrix */
						if (deptype_from != NULL || di_from == NULL) {
							dep_num   = 0;
							dep_count = unap[unapm_type][dep_remove_from_service_id][0];
							while (dep_num++ < dep_count) {
								if (unap[unapm_type][dep_remove_from_service_id][dep_num] == dep_remove_to_service_id) {
									unap[unapm_type][dep_remove_from_service_id][dep_num] =
										unap[unapm_type][dep_remove_from_service_id][dep_count--];
									if (deptype_from != NULL && depinfo_to != NULL)
										ewarn("Solving the loop by breaking %s %c> %s.",
											depinfo_to->service, deptype2char(unapm_type), depinfo_from->service);
								}
							}
							unap[unapm_type][dep_remove_from_service_id][0] = dep_count;
						}

						/* removing back dependencies */

						if (di_to == NULL)
							return;

						deptype_num = 0;
						while (deppairs[deptype_num].depend) {
							if (!strcmp(deppairs[deptype_num].depend, type)) {
								type_reverse = deppairs[deptype_num].addto;
								break;
							}
							deptype_num++;
						}

						if (deptype_to != NULL)
							rc_stringlist_delete(deptype_to->services, di_from->service);

						return;
					}

					if (svc_id2depinfo_bt != NULL) {
						item.key     = (void *)(long)service_id_from;
						item_pp      = tfind(&item, &svc_id2depinfo_bt, svc_id2depinfo_bt_compare);
						depinfo_from = (RC_DEPINFO *)((ENTRY *)*item_pp)->data;

						item.key     = (void *)(long)service_id_to;
						item_pp      = tfind(&item, &svc_id2depinfo_bt, svc_id2depinfo_bt_compare);
						depinfo_to   = (RC_DEPINFO *)((ENTRY *)*item_pp)->data;
					}

					/* Remove weak dependency */

					rc_deptree_remove_loopdependency(unap_matrix, service_id_from, service_id_to, depinfo_from, depinfo_to, "iuse",   UNAPM_USE);
					rc_deptree_remove_loopdependency(unap_matrix, service_id_from, service_id_to, depinfo_from, depinfo_to, "iafter", UNAPM_AFTER);

				}
			}
		}

		/* cleanup */

		tdestroy(btree, free);
		free(idid_to_break);
	}

	/* cleanup */
	{
		int i;

		i = 0;
		while (i < chain_count) {
			if (svc_id2depinfo_bt != NULL)
				free(chain_strs[i]);
			free(deptypes[i]);
			free(chains[i]);
			i++;
		}
		if (svc_id2depinfo_bt != NULL)
			free(chain_strs);
		free(deptypes);
		free(chains);
	}

	return LOOP_SOLVABLE;
}

/*! Mixing all dependencies to UNAPM_MIXED and expanding them (with dependencies of dependencies) in UNAPM_MIXED_EXPANDED
 * @param unap_matrix matrixes to scan ways to solve the loop
 * @param useneedafter_count total count of use/need/before/provide relations */
static void
rc_deptree_unapm_prepare_mixed(service_id_t **unap_matrix[UNAPM_MAX], unsigned int useneedafter_count) {
	service_id_t service_id;
	int onemorecycle;

	/* getting pre-matrix of all dependencies types together */

	service_id = 1;
	while (service_id < (useneedafter_count+1)) {
		service_id_t services_dst;
		unapm_type_t unapm_type;

		memset(unap_matrix[UNAPM_MIXED][service_id], 0, (useneedafter_count+1) * sizeof(**unap_matrix));
		services_dst = 0;
		unapm_type = 0;
		while (unapm_type < UNAPM_MIXED) {
			service_id_t services_src;

			services_src = unap_matrix[unapm_type][service_id][0];
			while (services_src)  {
				unap_matrix[UNAPM_MIXED][service_id][ ++services_dst ] =
					unap_matrix[unapm_type][service_id][ services_src-- ];
			}
			unapm_type++;
		}
		unap_matrix[UNAPM_MIXED][service_id][0] = services_dst;
		service_id++;
	}

	/* preparing full dependencies matrix */

	/* copying UNAPM_MIXED -> UNAPM_MIXED_EXPANDED */
	service_id = 1;
	while (service_id < (useneedafter_count+1)) {
		memcpy(unap_matrix[UNAPM_MIXED_EXPANDED][service_id], unap_matrix[UNAPM_MIXED][service_id], (useneedafter_count+1) * sizeof(**unap_matrix));
		service_id++;
	}

	do {
		onemorecycle = 0;

		/* direct way: service_id = 1 -> end */
		service_id = 1;
		while (service_id < (useneedafter_count+1))
			onemorecycle += rc_deptree_unapm_expandsdeps(unap_matrix[UNAPM_MIXED_EXPANDED], service_id++);

		/* reverse way: service_id = end -> 1 */
		while (--service_id)
			onemorecycle += rc_deptree_unapm_expandsdeps(unap_matrix[UNAPM_MIXED_EXPANDED], service_id);

	} while (onemorecycle);

	return;
}

/* This is a 7 phase operation
   Phase 1 is a shell script which loads each init script and config in turn
   and echos their dependency info to stdout
   Phase 2 takes that and populates a depinfo object with that data
   Phase 3 adds any provided services to the depinfo object
   Phase 4 scans that depinfo object and puts in backlinks
   Phase 5 removes broken before dependencies
   Phase 6 check for loops
   Phase 7 saves the depinfo object to disk
   */
bool
rc_deptree_update(void)
{
	FILE *fp;
	RC_DEPTREE *deptree, *providers;
	RC_DEPINFO *depinfo = NULL, *depinfo_np, *di;
	RC_DEPTYPE *deptype = NULL, *dt_np, *dt, *provide;
	RC_STRINGLIST *config, *types, *sorted, *visited;
	RC_STRING *s, *s2, *s2_np, *s3, *s4;
	char *line = NULL;
	size_t len = 0;
	char *depend, *depends, *service, *type, *nosys, *onosys;
	size_t i, k, l;
	bool retval = true;
	const char *sys = rc_sys();
	struct utsname uts;
	unsigned int useneedafter_count=0;

	/* Some init scripts need RC_LIBEXECDIR to source stuff
	   Ideally we should be setting our full env instead */
	if (!getenv("RC_LIBEXECDIR"))
		setenv("RC_LIBEXECDIR", RC_LIBEXECDIR, 0);

	if (uname(&uts) == 0)
		setenv("RC_UNAME", uts.sysname, 1);
	/* Phase 1 - source all init scripts and print dependencies */
	if (!(fp = popen(GENDEP, "r")))
		return false;

	deptree = xmalloc(sizeof(*deptree));
	TAILQ_INIT(deptree);
	config = rc_stringlist_new();
	while ((rc_getline(&line, &len, fp)))
	{
		depends = line;
		service = strsep(&depends, " ");
		if (!service || !*service)
			continue;

		type = strsep(&depends, " ");
		if (!depinfo || strcmp(depinfo->service, service) != 0) {
			deptype = NULL;
			depinfo = get_depinfo(deptree, service);
			if (!depinfo) {
				depinfo = xmalloc(sizeof(*depinfo));
				TAILQ_INIT(&depinfo->depends);
				depinfo->service = xstrdup(service);
				TAILQ_INSERT_TAIL(deptree, depinfo, entries);
			}
		}

		/* We may not have any depends */
		if (!type || !depends)
			continue;

		/* Get the type */
		if (strcmp(type, "config") != 0) {
			if (!deptype || strcmp(deptype->type, type) != 0)
				deptype = get_deptype(depinfo, type);
			if (!deptype) {
				deptype = xmalloc(sizeof(*deptype));
				deptype->type = xstrdup(type);
				deptype->services = rc_stringlist_new();
				TAILQ_INSERT_TAIL(&depinfo->depends, deptype, entries);
			}
		}

		/* Now add each depend to our type.
		   We do this individually so we handle multiple spaces gracefully */
		while ((depend = strsep(&depends, " ")))
		{
			if (depend[0] == 0)
				continue;

			if (strcmp(type, "config") == 0) {
				rc_stringlist_addu(config, depend);
				continue;
			}

			/* Don't provide ourself */
			if (strcmp(type, "iprovide") == 0 &&
			    strcmp(depend, service) == 0)
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
				rc_stringlist_delete(deptype->services, depend + 1);
				continue;
			}

			rc_stringlist_add(deptype->services, depend);

			/* We need to allow `after *; before local;` to work.
			 * Conversely, we need to allow 'before *; after modules' also */
			/* If we're before something, remove us from the after list */
			if (strcmp(type, "ibefore") == 0) {
				if ((dt = get_deptype(depinfo, "iafter")))
					rc_stringlist_delete(dt->services, depend);
			}
			/* If we're after something, remove us from the before list */
			if (strcmp(type, "iafter") == 0 ||
			    strcmp(type, "ineed") == 0 ||
			    strcmp(type, "iuse") == 0) {
				if ((dt = get_deptype(depinfo, "ibefore")))
					rc_stringlist_delete(dt->services, depend);
			}
		}
	}
	free(line);
	pclose(fp);

	/* Phase 2 - if we're a special system, remove services that don't
	 * work for them. This doesn't stop them from being run directly. */
	if (sys) {
		len = strlen(sys);
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

		TAILQ_FOREACH_SAFE(depinfo, deptree, entries, depinfo_np)
			if ((deptype = get_deptype(depinfo, "keyword")))
				TAILQ_FOREACH(s, deptype->services, entries)
					if (strcmp(s->value, nosys) == 0 ||
					    strcmp(s->value, onosys) == 0)
					{
						provide = get_deptype(depinfo, "iprovide");
						TAILQ_REMOVE(deptree, depinfo, entries);
						TAILQ_FOREACH(di, deptree, entries) {
							TAILQ_FOREACH_SAFE(dt, &di->depends, entries, dt_np) {
								rc_stringlist_delete(dt->services, depinfo->service);
								if (provide)
									TAILQ_FOREACH(s2, provide->services, entries)
										rc_stringlist_delete(dt->services, s2->value);
								if (!TAILQ_FIRST(dt->services)) {
									TAILQ_REMOVE(&di->depends, dt, entries);
									free(dt->type);
									free(dt->services);
									free(dt);
								}
							}
						}
					}
		free(nosys);
		free(onosys);
	}

	/* Phase 3 - add our providers to the tree */
	providers = xmalloc(sizeof(*providers));
	TAILQ_INIT(providers);
	TAILQ_FOREACH(depinfo, deptree, entries)
		if ((deptype = get_deptype(depinfo, "iprovide")))
			TAILQ_FOREACH(s, deptype->services, entries) {
				TAILQ_FOREACH(di, providers, entries)
					if (strcmp(di->service, s->value) == 0)
						break;
				if (!di) {
					di = xmalloc(sizeof(*di));
					TAILQ_INIT(&di->depends);
					di->service = xstrdup(s->value);
					TAILQ_INSERT_TAIL(providers, di, entries);
				}
			}
	TAILQ_CONCAT(deptree, providers, entries);
	free(providers);

	/* Phase 4 - backreference our depends */
	TAILQ_FOREACH(depinfo, deptree, entries)
		for (i = 0; deppairs[i].depend; i++) {
			deptype = get_deptype(depinfo, deppairs[i].depend);
			if (!deptype)
				continue;
			TAILQ_FOREACH(s, deptype->services, entries) {
				di = get_depinfo(deptree, s->value);
				if (!di) {
					if (strcmp(deptype->type, "ineed") == 0) {
						fprintf(stderr,
							 "Service `%s' needs non"
							 " existent service `%s'\n",
							 depinfo->service, s->value);
						dt = get_deptype(depinfo, "broken");
						if (!dt) {
							dt = xmalloc(sizeof(*dt));
							dt->type = xstrdup("broken");
							dt->services = rc_stringlist_new();
							TAILQ_INSERT_TAIL(&depinfo->depends, dt, entries);
						}
						rc_stringlist_addu(dt->services, s->value);
					}
					continue;
				}

				dt = get_deptype(di, deppairs[i].addto);
				if (!dt) {
					dt = xmalloc(sizeof(*dt));
					dt->type = xstrdup(deppairs[i].addto);
					dt->services = rc_stringlist_new();
					TAILQ_INSERT_TAIL(&di->depends, dt, entries);
				}
				rc_stringlist_addu(dt->services, depinfo->service);
			}
		}


	/* Phase 5 - Remove broken before directives */
	types = rc_stringlist_new();
	rc_stringlist_add(types, "ineed");
	rc_stringlist_add(types, "iuse");
	rc_stringlist_add(types, "iafter");
	TAILQ_FOREACH(depinfo, deptree, entries) {
		useneedafter_count++;

		deptype = get_deptype(depinfo, "ibefore");
		if (!deptype)
			continue;
		sorted = rc_stringlist_new();
		visited = rc_stringlist_new();
		visit_service(deptree, types, sorted, visited, depinfo,
			      NULL, 0);
		rc_stringlist_free(visited);
		TAILQ_FOREACH_SAFE(s2, deptype->services, entries, s2_np) {
			TAILQ_FOREACH(s3, sorted, entries) {
				di = get_depinfo(deptree, s3->value);
				if (!di)
					continue;
				if (strcmp(s2->value, s3->value) == 0) {
					dt = get_deptype(di, "iafter");
					if (dt)
						rc_stringlist_delete(dt->services, depinfo->service);
					break;
				}
				dt = get_deptype(di, "iprovide");
				if (!dt)
					continue;
				TAILQ_FOREACH(s4, dt->services, entries) {
					if (strcmp(s4->value, s2->value) == 0)
						break;
				}
				if (s4) {
					di = get_depinfo(deptree, s4->value);
					if (di) {
						dt = get_deptype(di, "iafter");
						if (dt)
							rc_stringlist_delete(dt->services, depinfo->service);
					}
					break;
				}
			}
			if (s3)
				rc_stringlist_delete(deptype->services, s2->value);
		}
		rc_stringlist_free(sorted);
	}
	rc_stringlist_free(types);

	/* Phase 6 - check for loops (non-recursive way) */
	{
		int loopfound;
		unapm_type_t unapm_type;
		service_id_t **unap_matrix[UNAPM_MAX];
		service_id_t service_id;
		void *svc_id2depinfo_bt = NULL;
		int loopsolver_counter = 0;

		hcreate(useneedafter_count*2);

		unapm_type = 0;
		while (unapm_type < UNAPM_MAX)
			unap_matrix[unapm_type++] = xmalloc(sizeof(*unap_matrix) * (useneedafter_count+1));

		/* preparing a hash-table: service_name -> service_id */
		service_id = 1;
		TAILQ_FOREACH(depinfo, deptree, entries) {
			ENTRY item, *item_p;

			item.key     = depinfo->service;
			item.data    = (void *)(long int)service_id;
			hsearch(item, ENTER);

			item_p       = xmalloc(sizeof(*item_p));
			item_p->key  = (void *)(long int)service_id;
			item_p->data = depinfo;
			tsearch(item_p, &svc_id2depinfo_bt, svc_id2depinfo_bt_compare);

			service_id++;
		}

		unap_matrix[0][0]    = xmalloc(sizeof(***unap_matrix)*1);
		unap_matrix[0][0][0] = service_id;

		/* getting dependencies pre-matrixes */
		service_id = 1;
		TAILQ_FOREACH(depinfo, deptree, entries) {
			rc_deptree_unapm_getdependencies(unap_matrix[UNAPM_USE],         useneedafter_count, service_id, "iuse",       depinfo);
			rc_deptree_unapm_getdependencies(unap_matrix[UNAPM_NEED],        useneedafter_count, service_id, "ineed",      depinfo);
			rc_deptree_unapm_getdependencies(unap_matrix[UNAPM_PROVIDEDBY],  useneedafter_count, service_id, "providedby", depinfo);
			rc_deptree_unapm_getdependencies(unap_matrix[UNAPM_AFTER],       useneedafter_count, service_id, "iafter",     depinfo);
			service_id++;
		}

		hdestroy();

		/* getting pre-matrix of all dependencies types together (allocating memory) */

		service_id = 1;
		while (service_id < (useneedafter_count+1)) {
			unap_matrix[UNAPM_MIXED         ][service_id] = xmalloc((useneedafter_count+1) * sizeof(**unap_matrix));
			unap_matrix[UNAPM_MIXED_EXPANDED][service_id] = xmalloc((useneedafter_count+1) * sizeof(**unap_matrix));
			service_id++;
		}

		do {
			loopfound = 0;

			/* updating UNAPM_MIXED and UNAPM_MIXED_EXPANDED in UNAP matrix */

			rc_deptree_unapm_prepare_mixed(unap_matrix, useneedafter_count);

			/* detecting and solving loop (non-recursive method) */
			/* the loop is a situation where service is depended on itself */
			service_id=1;
			while ((service_id < (useneedafter_count+1)) && !loopfound) {
				int dep_num, dep_count;

				dep_num = 0;
				dep_count = unap_matrix[UNAPM_MIXED_EXPANDED][service_id][0];
				while (dep_num < dep_count) {
					dep_num++;
					if (unap_matrix[UNAPM_MIXED_EXPANDED][service_id][dep_num] == service_id) {
						loopfound = rc_deptree_solve_loop(unap_matrix, service_id, svc_id2depinfo_bt, dep_num);
						loopsolver_counter++;
						break;
					}
				}

				service_id++;
			}
		} while (loopfound == LOOP_SOLVABLE && loopsolver_counter < LOOPSOLVER_LIMIT);

		if (loopsolver_counter >= LOOPSOLVER_LIMIT)
			eerror("Dependency loop solver reached iterations limit.");

		/* clean up */

		unapm_type = 0;
		while (unapm_type < UNAPM_MAX) {
			service_id=1;
			while (service_id < (useneedafter_count+1))
				free(unap_matrix[unapm_type][service_id++]);
			free(unap_matrix[unapm_type++]);
		}

		tdestroy(svc_id2depinfo_bt, free);
	}

	/* Phase 7 - save to disk
	   Now that we're purely in C, do we need to keep a shell parseable file?
	   I think yes as then it stays human readable
	   This works and should be entirely shell parseable provided that depend
	   names don't have any non shell variable characters in
	   */
	if ((fp = fopen(RC_DEPTREE_CACHE, "w"))) {
		i = 0;
		TAILQ_FOREACH(depinfo, deptree, entries) {
			fprintf(fp, "depinfo_%zu_service='%s'\n",
				i, depinfo->service);
			TAILQ_FOREACH(deptype, &depinfo->depends, entries) {
				k = 0;
				TAILQ_FOREACH(s, deptype->services, entries) {
					fprintf(fp,
						"depinfo_%zu_%s_%zu='%s'\n",
						i, deptype->type, k, s->value);
					k++;
				}
			}
			i++;
		}
		fclose(fp);
	} else {
		fprintf(stderr, "fopen `%s': %s\n",
			RC_DEPTREE_CACHE, strerror(errno));
		retval = false;
	}

	/* Save our external config files to disk */
	if (TAILQ_FIRST(config)) {
		if ((fp = fopen(RC_DEPCONFIG, "w"))) {
			TAILQ_FOREACH(s, config, entries)
				fprintf(fp, "%s\n", s->value);
			fclose(fp);
		} else {
			fprintf(stderr, "fopen `%s': %s\n",
				RC_DEPCONFIG, strerror(errno));
			retval = false;
		}
	} else {
		unlink(RC_DEPCONFIG);
	}

	rc_stringlist_free(config);
	rc_deptree_free(deptree);
	return retval;
}
librc_hidden_def(rc_deptree_update)
