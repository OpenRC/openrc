/*
   librc-depend
   rc service dependency and ordering
   Copyright 2006-2007 Gentoo Foundation
   */

#include "librc.h"

#define GENDEP	RC_LIBDIR "/sh/gendepends.sh"

/* We use this so we can pass our char array through many functions */
struct lhead
{
  char **list;
};

static char *get_shell_value (char *string)
{
  char *p = string;
  char *e;

  if (! string)
    return (NULL);

  if (*p == '\'')
    p++;

  e = p + strlen (p) - 1;
  if (*e == '\n')
    *e-- = 0;
  if (*e == '\'')
    *e-- = 0;

  if (*p != 0)
    return p;

  return (NULL);
}

void rc_free_deptree (rc_depinfo_t *deptree)
{
  rc_depinfo_t *di = deptree;
  while (di)
    {
      rc_depinfo_t *dip = di->next;
      rc_deptype_t *dt = di->depends;
      free (di->service);
      while (dt)
        {
          rc_deptype_t *dtp = dt->next;
          free (dt->type);  
          rc_strlist_free (dt->services);
          free (dt);
          dt = dtp;
        }
      free (di);
      di = dip;
    }
}
librc_hidden_def(rc_free_deptree)

rc_depinfo_t *rc_load_deptree (void)
{
  FILE *fp;
  rc_depinfo_t *deptree = NULL;
  rc_depinfo_t *depinfo = NULL;
  rc_deptype_t *deptype = NULL;
  char buffer [RC_LINEBUFFER];
  char *type;
  char *p;
  char *e;
  int i;

  /* Update our deptree, but only if we need too */
  rc_update_deptree (false);

  if (! (fp = fopen (RC_DEPTREE, "r")))
    return (NULL);

  while (fgets (buffer, RC_LINEBUFFER, fp))
    {
      p = buffer;
      e = strsep (&p, "_");
      if (! e || strcmp (e, "depinfo") != 0)
        continue;

      e = strsep (&p, "_");
      if (! e || sscanf (e, "%d", &i) != 1)
        continue;

      if (! (type = strsep (&p, "_=")))
        continue;

      if (strcmp (type, "service") == 0)
        {
          /* Sanity */
          e = get_shell_value (p);
          if (! e || strlen (e) == 0)
            continue;

          if (! deptree)
            {
              deptree = rc_xmalloc (sizeof (rc_depinfo_t));
              depinfo = deptree;
            }
          else
            {
              depinfo->next = rc_xmalloc (sizeof (rc_depinfo_t));
              depinfo = depinfo->next;
            }
          memset (depinfo, 0, sizeof (rc_depinfo_t));
          depinfo->service = rc_xstrdup (e);
          deptype = NULL;
          continue;
        }

      e = strsep (&p, "=");
      if (! e || sscanf (e, "%d", &i) != 1)
        continue;

      /* Sanity */
      e = get_shell_value (p);
      if (! e || strlen (e) == 0)
        continue;

      if (! deptype)
        {
          depinfo->depends = rc_xmalloc (sizeof (rc_deptype_t));
          deptype = depinfo->depends;
          memset (deptype, 0, sizeof (rc_deptype_t));
        }
      else
        if (strcmp (deptype->type, type) != 0)
          {
            deptype->next = rc_xmalloc (sizeof (rc_deptype_t));
            deptype = deptype->next;
            memset (deptype, 0, sizeof (rc_deptype_t));
          }

      if (! deptype->type)
        deptype->type = rc_xstrdup (type);

      deptype->services = rc_strlist_addsort (deptype->services, e);
    }
  fclose (fp);

  return (deptree);
}
librc_hidden_def(rc_load_deptree)

rc_depinfo_t *rc_get_depinfo (rc_depinfo_t *deptree, const char *service)
{
  rc_depinfo_t *di;

  if (! deptree || ! service)
    return (NULL);

  for (di = deptree; di; di = di->next)
    if (strcmp (di->service, service) == 0)
      return (di);

  return (NULL);
}
librc_hidden_def(rc_get_depinfo)

rc_deptype_t *rc_get_deptype (rc_depinfo_t *depinfo, const char *type)
{
  rc_deptype_t *dt;

  if (! depinfo || !type)
    return (NULL);

  for (dt = depinfo->depends; dt; dt = dt->next)
    if (strcmp (dt->type, type) == 0)
      return (dt);

  return (NULL);
}
librc_hidden_def(rc_get_deptype)

static bool valid_service (const char *runlevel, const char *service)
{
  return ((strcmp (runlevel, RC_LEVEL_BOOT) != 0 &&
           rc_service_in_runlevel (service, RC_LEVEL_BOOT)) ||
          rc_service_in_runlevel (service, runlevel) ||
          rc_service_state (service, rc_service_coldplugged) ||
          rc_service_state (service, rc_service_started));
}

static bool get_provided1 (const char *runlevel, struct lhead *providers,
                           rc_deptype_t *deptype,
                           const char *level, bool coldplugged,
						   rc_service_state_t state)
{
  char *service;
  int i;
  bool retval = false;
  char *bootlevel = getenv ("RC_BOOTLEVEL");

  STRLIST_FOREACH (deptype->services, service, i)
    {
      bool ok = true;
      if (level)
        ok = rc_service_in_runlevel (service, level);
      else if (coldplugged)
        ok = (rc_service_state (service, rc_service_coldplugged) &&
              ! rc_service_in_runlevel (service, runlevel) &&
              ! rc_service_in_runlevel (service, bootlevel));

      if (! ok)
        continue;

	  switch (state) {
		case rc_service_started:
			ok = rc_service_state (service, state);
			break;
		case rc_service_inactive:
		case rc_service_starting:
		case rc_service_stopping:
			ok = (rc_service_state (service, rc_service_starting) ||
				  rc_service_state (service, rc_service_stopping) ||
				  rc_service_state (service, rc_service_inactive));
			break;
		default:
			break;
	  }
      
	  if (! ok)
        continue;

      retval = true;
      providers->list = rc_strlist_add (providers->list, service);
    }

  return (retval);
}

/* Work out if a service is provided by another service.
   For example metalog provides logger.
   We need to be able to handle syslogd providing logger too.
   We do this by checking whats running, then what's starting/stopping,
   then what's run in the runlevels and finally alphabetical order.

   If there are any bugs in rc-depend, they will probably be here as
   provided dependancy can change depending on runlevel state.
   */
static char **get_provided (rc_depinfo_t *deptree, rc_depinfo_t *depinfo,
                            const char *runlevel, int options)
{
  rc_deptype_t *dt;
  struct lhead providers; 
  char *service;
  int i;
  char *bootlevel;

  if (! deptree || ! depinfo)
    return (NULL);
  if (rc_service_exists (depinfo->service))
    return (NULL);

  dt = rc_get_deptype (depinfo, "providedby");
  if (! dt)
    return (NULL);

  memset (&providers, 0, sizeof (struct lhead));
  /* If we are stopping then all depends are true, regardless of state.
     This is especially true for net services as they could force a restart
     of the local dns resolver which may depend on net. */
  if (options & RC_DEP_STOP)
    {
      STRLIST_FOREACH (dt->services, service, i)
       providers.list = rc_strlist_add (providers.list, service);

      return (providers.list);
    }

  /* If we're strict, then only use what we have in our runlevel */
  if (options & RC_DEP_STRICT)
    {
      STRLIST_FOREACH (dt->services, service, i)
       if (rc_service_in_runlevel (service, runlevel))
         providers.list = rc_strlist_add (providers.list, service);

      if (providers.list)
        return (providers.list);
    }

  /* OK, we're not strict or there were no services in our runlevel.
     This is now where the logic gets a little fuzzy :)
     If there is >1 running service then we return NULL.
     We do this so we don't hang around waiting for inactive services and
     our need has already been satisfied as it's not strict.
     We apply this to our runlevel, coldplugged services, then bootlevel
     and finally any running.*/
#define DO \
  if (providers.list && providers.list[0] && providers.list[1]) \
    { \
      rc_strlist_free (providers.list); \
      return (NULL); \
    } \
  else if (providers.list)  \
  return providers.list; \

  /* Anything in the runlevel has to come first */
  if (get_provided1 (runlevel, &providers, dt, runlevel, false, rc_service_started))
    { DO }
  if (get_provided1 (runlevel, &providers, dt, runlevel, false, rc_service_starting))
	return (providers.list);
  if (get_provided1 (runlevel, &providers, dt, runlevel, false, rc_service_stopped))
    return (providers.list);

  /* Check coldplugged services */
  if (get_provided1 (runlevel, &providers, dt, NULL, true, rc_service_started))
    { DO }
  if (get_provided1 (runlevel, &providers, dt, NULL, true, rc_service_starting))
	return (providers.list);

  /* Check bootlevel if we're not in it */
  bootlevel = getenv ("RC_BOOTLEVEL");
  if (bootlevel && strcmp (runlevel, bootlevel) != 0)
    {
      if (get_provided1 (runlevel, &providers, dt, bootlevel, false, rc_service_started))
        { DO }
      if (get_provided1 (runlevel, &providers, dt, bootlevel, false, rc_service_starting))
        return (providers.list);
	}

  /* Check coldplugged services */
  if (get_provided1 (runlevel, &providers, dt, NULL, true, rc_service_stopped))

  /* Check manually started */
  if (get_provided1 (runlevel, &providers, dt, NULL, false, rc_service_started))
    { DO }
  if (get_provided1 (runlevel, &providers, dt, NULL, false, rc_service_starting))
    return (providers.list);

  /* Nothing started then. OK, lets get the stopped services */
  if (get_provided1 (runlevel, &providers, dt, runlevel, false, rc_service_stopped))
    return (providers.list);

  if (bootlevel && (strcmp (runlevel, bootlevel) != 0)
      && (get_provided1 (runlevel, &providers, dt, bootlevel, false, rc_service_stopped)))
    return (providers.list);

  /* Still nothing? OK, list all services */
  STRLIST_FOREACH (dt->services, service, i)
   providers.list = rc_strlist_add (providers.list, service);

  return (providers.list);
}

static void visit_service (rc_depinfo_t *deptree, char **types,
                           struct lhead *sorted, struct lhead *visited,
                           rc_depinfo_t *depinfo,
                           const char *runlevel, int options)
{
  int i, j, k;
  char *lp, *item;
  char *service;
  rc_depinfo_t *di;
  rc_deptype_t *dt;
  char **provides;
  char *svcname;

  if (! deptree || !sorted || !visited || !depinfo)
    return;

  /* Check if we have already visited this service or not */
  STRLIST_FOREACH (visited->list, item, i)
   if (strcmp (item, depinfo->service) == 0)
     return;

  /* Add ourselves as a visited service */
  visited->list = rc_strlist_add (visited->list, depinfo->service);

  STRLIST_FOREACH (types, item, i)
    {
      if ((dt = rc_get_deptype (depinfo, item)))
        {
          STRLIST_FOREACH (dt->services, service, j)
            {
              if (! options & RC_DEP_TRACE || strcmp (item, "iprovide") == 0)
                {
                  sorted->list = rc_strlist_add (sorted->list, service);
                  continue;
                }

              di = rc_get_depinfo (deptree, service);
              if ((provides = get_provided (deptree, di, runlevel, options)))
                {
                  STRLIST_FOREACH (provides, lp, k) 
                    {
                      di = rc_get_depinfo (deptree, lp);
                      if (di && (strcmp (item, "ineed") == 0 ||
                                 valid_service (runlevel, di->service)))
                        visit_service (deptree, types, sorted, visited, di,
                                       runlevel, options | RC_DEP_TRACE);
                    }
                  rc_strlist_free (provides);
                }
              else
                if (di && (strcmp (item, "ineed") == 0 ||
                           valid_service (runlevel, service)))
                  visit_service (deptree, types, sorted, visited, di,
                                 runlevel, options | RC_DEP_TRACE);
            }
        }
    }

  /* Now visit the stuff we provide for */
  if (options & RC_DEP_TRACE && (dt = rc_get_deptype (depinfo, "iprovide")))
    {
      STRLIST_FOREACH (dt->services, service, i)
        {
          if ((di = rc_get_depinfo (deptree, service)))
            if ((provides = get_provided (deptree, di, runlevel, options)))
              {
                STRLIST_FOREACH (provides, lp, j)
                 if (strcmp (lp, depinfo->service) == 0)
                   {
                     visit_service (deptree, types, sorted, visited, di,
                                    runlevel, options | RC_DEP_TRACE);
                     break;
                   }
                rc_strlist_free (provides);
              }
        }
    }

  /* We've visited everything we need, so add ourselves unless we
     are also the service calling us or we are provided by something */
  svcname = getenv("SVCNAME");
  if (! svcname || strcmp (svcname, depinfo->service) != 0)
    if (! rc_get_deptype (depinfo, "providedby"))
      sorted->list = rc_strlist_add (sorted->list, depinfo->service);
}

char **rc_get_depends (rc_depinfo_t *deptree,
                       char **types, char **services,
                       const char *runlevel, int options)
{  
  struct lhead sorted;
  struct lhead visited;
  rc_depinfo_t *di;
  char *service;
  int i;

  if (! deptree || ! types || ! services)
    return (NULL);

  memset (&sorted, 0, sizeof (struct lhead));
  memset (&visited, 0, sizeof (struct lhead));

  STRLIST_FOREACH (services, service, i)
    {
      di = rc_get_depinfo (deptree, service);
      visit_service (deptree, types, &sorted, &visited, di, runlevel, options);
    }

  rc_strlist_free (visited.list);
  return (sorted.list);
}
librc_hidden_def(rc_get_depends)

char **rc_order_services (rc_depinfo_t *deptree, const char *runlevel,
                          int options)
{
  char **list = NULL;
  char **types = NULL;
  char **services = NULL;
  bool reverse = false;

  if (! runlevel)
    return (NULL);

  /* When shutting down, list all running services */
  if (strcmp (runlevel, RC_LEVEL_SINGLE) == 0 ||
      strcmp (runlevel, RC_LEVEL_SHUTDOWN) == 0 ||
      strcmp (runlevel, RC_LEVEL_REBOOT) == 0)
    {
      list = rc_ls_dir (list, RC_SVCDIR_STARTING, RC_LS_INITD);
      list = rc_ls_dir (list, RC_SVCDIR_INACTIVE, RC_LS_INITD);
      list = rc_ls_dir (list, RC_SVCDIR_STARTED, RC_LS_INITD);
      reverse = true;
    }
  else
    {
      list = rc_services_in_runlevel (runlevel);

      /* Add coldplugged services */
      list = rc_ls_dir (list, RC_SVCDIR_COLDPLUGGED, RC_LS_INITD);

      /* If we're not the boot runlevel then add that too */
      if (strcmp (runlevel, RC_LEVEL_BOOT) != 0)
        {
          char *path = rc_strcatpaths (RC_RUNLEVELDIR, RC_LEVEL_BOOT,
                                       (char *) NULL);
          list = rc_ls_dir (list, path, RC_LS_INITD);
          free (path);
        }
    } 

  /* Now we have our lists, we need to pull in any dependencies
     and order them */
  types = rc_strlist_add (NULL, "ineed");
  types = rc_strlist_add (types, "iuse");
  types = rc_strlist_add (types, "iafter");
  services = rc_get_depends (deptree, types, list, runlevel,
                             RC_DEP_STRICT | RC_DEP_TRACE | options);
  rc_strlist_free (list);
  rc_strlist_free (types);

  if (reverse)
    rc_strlist_reverse (services);

  return (services);
}
librc_hidden_def(rc_order_services)

static bool is_newer_than (const char *file, const char *target)
{
  struct stat buf;
  time_t mtime;

  if (stat (file, &buf) != 0 || buf.st_size == 0)
    return (false);
  mtime = buf.st_mtime;

  /* Of course we are newever than targets that don't exist
     Such as broken symlinks */
  if (stat (target, &buf) != 0)
    return (true);

  if (mtime < buf.st_mtime)
    return (false);

  if (rc_is_dir (target))
    {
      char **targets = rc_ls_dir (NULL, target, 0);
      char *t;
      int i;
      bool newer = true;
      STRLIST_FOREACH (targets, t, i)
        {
          char *path = rc_strcatpaths (target, t, (char *) NULL);
          newer = is_newer_than (file, path);
          free (path);
          if (! newer)
            break;
        }
      rc_strlist_free (targets);
      return (newer);
    }

  return (true);
}

typedef struct deppair
{
  const char *depend;
  const char *addto;
} deppair_t;

static const deppair_t deppairs[] = {
    { "ineed",		"needsme" },
    { "iuse",		"usesme" },
    { "iafter",		"ibefore" },
    { "ibefore",	"iafter" },
    { "iprovide",	"providedby" },
    { NULL, NULL }
};

static const char *depdirs[] =
{
  RC_SVCDIR "starting",
  RC_SVCDIR "started",
  RC_SVCDIR "stopping",
  RC_SVCDIR "inactive",
  RC_SVCDIR "wasinactive",
  RC_SVCDIR "failed",
  RC_SVCDIR "coldplugged",
  RC_SVCDIR "daemons",
  RC_SVCDIR "options",
  RC_SVCDIR "exclusive",
  RC_SVCDIR "scheduled",
  RC_SVCDIR "ebuffer",
  NULL
};

/* This is a 5 phase operation
   Phase 1 is a shell script which loads each init script and config in turn
   and echos their dependency info to stdout
   Phase 2 takes that and populates a depinfo object with that data
   Phase 3 adds any provided services to the depinfo object
   Phase 4 scans that depinfo object and puts in backlinks
   Phase 5 saves the depinfo object to disk
   */
int rc_update_deptree (bool force)
{
  char *depends;
  char *service;
  char *type;
  char *depend;
  int retval = 0;
  FILE *fp;
  rc_depinfo_t *deptree;
  rc_depinfo_t *depinfo;
  rc_depinfo_t *di;
  rc_depinfo_t *last_depinfo = NULL;
  rc_deptype_t *deptype;
  rc_deptype_t *dt;
  rc_deptype_t *last_deptype = NULL;
  char buffer[RC_LINEBUFFER];
  int len;
  int i;
  int j;
  int k;
  bool already_added;

  /* Create base directories if needed */
  for (i = 0; depdirs[i]; i++)
    if (! rc_is_dir (depdirs[i]))
      if (mkdir (depdirs[i], 0755) != 0)
        eerrorx ("mkdir `%s': %s", depdirs[i], strerror (errno));

  if (! force)
    if (is_newer_than (RC_DEPTREE, RC_INITDIR) &&
        is_newer_than (RC_DEPTREE, RC_CONFDIR) &&
        is_newer_than (RC_DEPTREE, "/etc/rc.conf"))
      return 0;

  ebegin ("Caching service dependencies");

  /* Some init scripts need RC_LIBDIR to source stuff
     Ideally we should be setting our full env instead */
  if (! getenv ("RC_LIBDIR"))
    setenv ("RC_LIBDIR", RC_LIBDIR, 0);

  /* Phase 1 */
  if (! (fp = popen (GENDEP, "r")))
    eerrorx ("popen: %s", strerror (errno));

  deptree = rc_xmalloc (sizeof (rc_depinfo_t));
  memset (deptree, 0, sizeof (rc_depinfo_t));
  memset (buffer, 0, RC_LINEBUFFER);

  /* Phase 2 */
  while (fgets (buffer, RC_LINEBUFFER, fp))
    {
      /* Trim the newline */
      if (buffer[strlen (buffer) - 1] == '\n')
        buffer[strlen(buffer) -1] = 0;

      depends = buffer;
      service = strsep (&depends, " ");
      if (! service)
        continue;
      type = strsep (&depends, " ");

      for (depinfo = deptree; depinfo; depinfo = depinfo->next)
        {
          last_depinfo = depinfo;
          if (depinfo->service && strcmp (depinfo->service, service) == 0)
            break;
        }

      if (! depinfo)
        {
          if (! last_depinfo->service)
            depinfo = last_depinfo;
          else
            {
              last_depinfo->next = rc_xmalloc (sizeof (rc_depinfo_t));
              depinfo = last_depinfo->next;
            }
          memset (depinfo, 0, sizeof (rc_depinfo_t));
          depinfo->service = rc_xstrdup (service);
        }

      /* We may not have any depends */
      if (! type || ! depends)
        continue;

      last_deptype = NULL;
      for (deptype = depinfo->depends; deptype; deptype = deptype->next)
        {
          last_deptype = deptype;
          if (strcmp (deptype->type, type) == 0)
            break;
        }

      if (! deptype)
        {
          if (! last_deptype)
            {
              depinfo->depends = rc_xmalloc (sizeof (rc_deptype_t));
              deptype = depinfo->depends;
            }
          else
            {
              last_deptype->next = rc_xmalloc (sizeof (rc_deptype_t));
              deptype = last_deptype->next;
            }
          memset (deptype, 0, sizeof (rc_deptype_t));
          deptype->type = rc_xstrdup (type);
        }

      /* Now add each depend to our type.
         We do this individually so we handle multiple spaces gracefully */
      while ((depend = strsep (&depends, " ")))
        {
          if (depend[0] == 0)
            continue;

          /* .sh files are not init scripts */
          len = strlen (depend);
          if (len > 2 &&
              depend[len - 3] == '.' &&
              depend[len - 2] == 's' &&
              depend[len - 1] == 'h')
            continue;

          deptype->services = rc_strlist_addsort (deptype->services, depend);
        }

    }
  pclose (fp);

  /* Phase 3 - add our providors to the tree */
  for (depinfo = deptree; depinfo; depinfo = depinfo->next)
    {
      if ((deptype = rc_get_deptype (depinfo, "iprovide")))
        STRLIST_FOREACH (deptype->services, service, i)
          {
            for (di = deptree; di; di = di->next)
              {
                last_depinfo = di;
                if (strcmp (di->service, service) == 0)
                  break;
              }
            if (! di)
              {
                last_depinfo->next = rc_xmalloc (sizeof (rc_depinfo_t));
                di = last_depinfo->next;
                memset (di, 0, sizeof (rc_depinfo_t));
                di->service = rc_xstrdup (service);
              }
          }
    }

  /* Phase 4 - backreference our depends */
  for (depinfo = deptree; depinfo; depinfo = depinfo->next)
    {
      for (i = 0; deppairs[i].depend; i++)
        {
          deptype = rc_get_deptype (depinfo, deppairs[i].depend);
          if (! deptype)
            continue;

          STRLIST_FOREACH (deptype->services, service, j)
            {
              di = rc_get_depinfo (deptree, service);
              if (! di)
                {
                  if (strcmp (deptype->type, "ineed") == 0)
                    {
                      eerror ("Service `%s' needs non existant service `%s'",
                              depinfo->service, service);
                      retval = -1;
                    } 
                  continue;
                }

              /* Add our deptype now */
              last_deptype = NULL;
              for (dt = di->depends; dt; dt = dt->next)
                {
                  last_deptype = dt;
                  if (strcmp (dt->type, deppairs[i].addto) == 0)
                    break;
                }
              if (! dt)
                {
                  if (! last_deptype)
                    {
                      di->depends = rc_xmalloc (sizeof (rc_deptype_t));
                      dt = di->depends;
                    }
                  else
                    {
                      last_deptype->next = rc_xmalloc (sizeof (rc_deptype_t));
                      dt = last_deptype->next;
                    }
                  memset (dt, 0, sizeof (rc_deptype_t));
                  dt->type = rc_xstrdup (deppairs[i].addto);
                }

              already_added = false;
              STRLIST_FOREACH (dt->services, service, k)
               if (strcmp (service, depinfo->service) == 0)
                 {
                   already_added = true;
                   break;
                 }

              if (! already_added)
                dt->services = rc_strlist_addsort (dt->services,
                                                   depinfo->service);
            }
        }
    }

  /* Phase 5 - save to disk
     Now that we're purely in C, do we need to keep a shell parseable file?
     I think yes as then it stays human readable
     This works and should be entirely shell parseable provided that depend
     names don't have any non shell variable characters in
     */
  if (! (fp = fopen (RC_DEPTREE, "w")))
    eerror ("fopen `%s': %s", RC_DEPTREE, strerror (errno));
  else
    {
      i = 0;
      for (depinfo = deptree; depinfo; depinfo = depinfo->next)
        {
          fprintf (fp, "depinfo_%d_service='%s'\n", i, depinfo->service);
          for (deptype = depinfo->depends; deptype; deptype = deptype->next)
            {
              k = 0;
              STRLIST_FOREACH (deptype->services, service, j)
                {
                  fprintf (fp, "depinfo_%d_%s_%d='%s'\n", i, deptype->type,
                           k, service);
                  k++;
                }
            }
          i++;
        }
      fclose (fp);
    }

  rc_free_deptree (deptree);

  eend (retval, "Failed to update the service dependency tree");
  return (retval);
}
librc_hidden_def(rc_update_deptree)
