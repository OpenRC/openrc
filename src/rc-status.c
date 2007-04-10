/*
   rc-status
   Display the status of the services in runlevels
   Copyright 2007 Gentoo Foundation
   Released under the GPLv2
   */

#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "einfo.h"
#include "rc.h"
#include "rc-misc.h"
#include "strlist.h"

static void print_level (char *level)
{
  printf ("Runlevel: ");
  PEINFO_HILITE;
  printf ("%s\n", level);
  PEINFO_NORMAL;
}

static void print_service (char *service)
{
  char status[10];
  int cols =  printf (" %s\n", service);
  einfo_color_t color = einfo_bad;

  if (rc_service_state (service, rc_service_stopping))
    snprintf (status, sizeof (status),   "stopping ");
  else if (rc_service_state (service, rc_service_starting))
    {
      snprintf (status, sizeof (status), "starting ");
      color = einfo_warn;
    }
  else if (rc_service_state (service, rc_service_inactive))
    {
      snprintf (status, sizeof (status), "inactive ");
      color = einfo_warn;
    }
  else if (geteuid () == 0 && rc_service_state (service, rc_service_crashed))
    snprintf (status, sizeof (status),   " crashed ");
  else if (rc_service_state (service, rc_service_started))
    {
      snprintf (status, sizeof (status), " started ");
      color = einfo_good;
    }
  else if (rc_service_state (service, rc_service_scheduled))
    {
      snprintf (status, sizeof (status), "scheduled");
      color = einfo_warn;
    }
  else
    snprintf (status, sizeof (status),   " stopped ");
  ebracket (cols, color, status);
}

int main (int argc, char **argv)
{
  char **levels = NULL; 
  char **services = NULL;
  char *level;
  char *service;
  char c;
  int option_index = 0;
  int i;
  int j;

  const struct option longopts[] =
    {
        {"all", no_argument, NULL, 'a'},
        {"list", no_argument, NULL, 'l'},
        {"servicelist", no_argument, NULL, 's'},
        {"unused", no_argument, NULL, 'u'},
        {NULL, 0, NULL, 0}
    };

  while ((c = getopt_long(argc, argv, "alsu", longopts, &option_index)) != -1)
    switch (c)
      {
      case 'a':
        levels = rc_get_runlevels ();
        break;
      case 'l':
        levels = rc_get_runlevels ();
        STRLIST_FOREACH (levels, level, i)
         printf ("%s\n", level);
        rc_strlist_free (levels);
        exit (EXIT_SUCCESS);
      case 's':
        services = rc_services_in_runlevel (NULL);
        STRLIST_FOREACH (services, service, i)
         print_service (service);
        rc_strlist_free (services);
        exit (EXIT_SUCCESS);
      case 'u':
        services = rc_services_in_runlevel (NULL);
        levels = rc_get_runlevels ();
        STRLIST_FOREACH (services, service, i)
          {
            bool found = false;
            STRLIST_FOREACH (levels, level, j)
             if (rc_service_in_runlevel (service, level))
               {
                 found = true;
                 break;
               }
            if (! found)
              print_service (service);
          }
        rc_strlist_free (levels);
        rc_strlist_free (services);
        exit (EXIT_SUCCESS);
      case '?':
        exit (EXIT_FAILURE);
      default:
        exit (EXIT_FAILURE);
      }

  while (optind < argc)
    levels = rc_strlist_add (levels, argv[optind++]);

  if (! levels)
    levels = rc_strlist_add (NULL, rc_get_runlevel ());

  STRLIST_FOREACH (levels, level, i)
    {
      print_level (level);
      services = rc_services_in_runlevel (level);
      STRLIST_FOREACH (services, service, j)
       print_service (service);
      rc_strlist_free (services);
    }

  rc_strlist_free (levels);

  return (EXIT_SUCCESS);
}
