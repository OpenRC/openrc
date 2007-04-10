/*
   rc-update
   Manage init scripts and runlevels
   Copyright 2007 Gentoo Foundation
   Released under the GPLv2
   */

#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "einfo.h"
#include "rc.h"
#include "rc-misc.h"
#include "strlist.h"

static char *applet = NULL;

static bool add (const char *runlevel, const char *service)
{
  bool retval = true;

  if (! rc_runlevel_exists (runlevel))
    {
      ewarn ("runlevel `%s' does not exist", runlevel);
      return (false);
    }
  if (rc_service_in_runlevel (service, runlevel))
    {
      ewarn ("%s already installed in runlevel `%s'; skipping",
             service, runlevel);
      return (false);
    }

  if (rc_service_add (runlevel, service))
    einfo ("%s added to runlevel %s", service, runlevel);
  else
    {
      eerror ("%s: failed to add service `%s' to runlevel `%s': %s",
              applet, service, runlevel, strerror (errno));
      retval = false;
    }

  return (retval);
}

int main (int argc, char **argv)
{
  int i;
  int j;
  char *service;
  char **runlevels = NULL;
  char *runlevel;

  applet = argv[0];
  if (argc < 2 ||
      strcmp (argv[1], "show") == 0 ||
      strcmp (argv[1], "-s") == 0)
    {
      bool verbose = false;
      char **services = rc_services_in_runlevel (NULL);

      for (i = 2; i < argc; i++)
        {
          if (strcmp (argv[i], "--verbose") == 0 ||
              strcmp (argv[i], "-v") == 0)
            verbose = true;
          else
            runlevels = rc_strlist_add (runlevels, argv[i]);
        }

      if (! runlevels)
        runlevels = rc_get_runlevels ();

      STRLIST_FOREACH (services, service, i)
        {
          char **in = NULL;
          bool inone = false;

          STRLIST_FOREACH (runlevels, runlevel, j)
            {
              if (rc_service_in_runlevel (service, runlevel))
                {
                  in = rc_strlist_add (in, runlevel);
                  inone = true;
                }
              else
                {
                  char buffer[PATH_MAX];
                  memset (buffer, ' ', strlen (runlevel));
                  buffer[strlen (runlevel)] = 0;
                  in = rc_strlist_add (in, buffer);
                }
            }

          if (! inone && ! verbose)
            continue;

          printf (" %20s |", service);
          STRLIST_FOREACH (in, runlevel, j)
           printf (" %s", runlevel);
          printf ("\n");
        }

      return (EXIT_SUCCESS);
    }

  if (geteuid () != 0)
    eerrorx ("%s: must be root to add or delete services from runlevels",
             applet);

  if (! (service = argv[2]))
    eerrorx ("%s: no service specified", applet);

  if (strcmp (argv[1], "add") == 0 ||
      strcmp (argv[1], "-a") == 0)
    {
      if (! service) 
        eerrorx ("%s: no service specified", applet);
      if (! rc_service_exists (service))
        eerrorx ("%s: service `%s' does not exist", applet, service);

      if (argc < 4)
        add (rc_get_runlevel (), service);

      for (i = 3; i < argc; i++)
        add (argv[i], service);

      return (EXIT_SUCCESS);
    }

  if (strcmp (argv[1], "delete") == 0 ||
      strcmp (argv[1], "del") == 0 ||
      strcmp (argv[1], "-d") == 0)
    {
      for (i = 3; i < argc; i++)
        runlevels = rc_strlist_add (runlevels, argv[i]);

      if (! runlevels)
        runlevels = rc_strlist_add (runlevels, rc_get_runlevel ()); 

      STRLIST_FOREACH (runlevels, runlevel, i)
        {
          if (rc_service_in_runlevel (service, runlevel))
            {
              if (rc_service_delete (runlevel, service))
                einfo ("%s removed from runlevel %s", service, runlevel);
              else
                eerror ("%s: failed to remove service `%s' from runlevel `%s': %s",
                        applet, service, runlevel, strerror (errno));
            }
        }

      return (EXIT_SUCCESS);
    }

  eerrorx ("%s: unknown command `%s'", applet, argv[1]);
}
