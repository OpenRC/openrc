/*
   splash.c

   Splash plugin for the Gentoo RC sytsem.
   splashutils needs to be re-written to support our new system.
   Until then, we provide this compatible module which calls the
   legacy bash scripts which is nasty. And slow.

   For any themes that use scripts, such as the live-cd theme,
   they will have to source /sbin/splash-functions.sh themselves like so

   if ! type splash >/dev/null 2>/dev/null ; then
      . /sbin/splash-functions.sh
   fi
   
   */

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <rc.h>

#ifndef LIBDIR
#  define LIBDIR "lib"
#endif

#define SPLASH_CACHEDIR "/" LIBDIR "/splash/cache"

#define SPLASH_CMD "bash -c 'export SOFTLEVEL='%s'; export BOOTLEVEL=${RC_BOOTLEVEL}; export DEFAULTLEVEL=${RC_DEFAULTLEVEL}; export svcdir=${RC_SVCDIR}; add_suffix() { echo \"$@\"; }; . /etc/init.d/functions.sh; . /sbin/splash-functions.sh; splash %s %s %s'"

int _splash_hook (rc_hook_t hook, const char *name);

static int _do_splash (const char *cmd, const char *arg1, const char *arg2)
{
  char *c;
  int l;
  char *soft = getenv ("RC_SOFTLEVEL");

  if (! cmd || ! soft)
    return (-1);

  l = strlen (SPLASH_CMD) + strlen (soft) + strlen (cmd);
  if (arg1)
    l += strlen (arg1);
  if (arg2)
    l += strlen (arg2);
  c = malloc (sizeof (char *) * l);
  if (! c)
    return (-1);

  snprintf (c, l, SPLASH_CMD,
	    arg1 ? strcmp (arg1, RC_LEVEL_SYSINIT) == 0 ? RC_LEVEL_BOOT : soft : soft,
	    cmd, arg1 ? arg1 : "", arg2 ? arg2 : "");
  l = system (c);
  free (c);
  return (l);
}

int _splash_hook (rc_hook_t hook, const char *name)
{
  switch (hook)
    {
    case rc_hook_runlevel_stop_in:
      if (strcmp (name, RC_LEVEL_SYSINIT) != 0)
	return (_do_splash ("rc_init", name, NULL));
      break;
    case rc_hook_runlevel_start_out:
      if (strcmp (name, RC_LEVEL_SYSINIT) == 0)
	return (_do_splash ("rc_init", name, NULL));
      else
	return (_do_splash ("rc_exit", name, NULL));
    default: ;
    }

  /* We don't care about splash unless we're changing runlevels */
  if (! rc_runlevel_starting () && 
      ! rc_runlevel_stopping ())
    return (0);

  switch (hook)
    {
    case rc_hook_service_stop_in:
      /* We need to stop localmount from unmounting our cache dir.
	 Luckily plugins can add to the unmount list. */
      if (name && strcmp (name, "localmount") == 0)
	{
	  char *umounts = getenv ("RC_NO_UMOUNTS");
	  char *new;
	  int i = strlen (SPLASH_CACHEDIR) + 1;

	  if (umounts)
	    i += strlen (umounts) + 1;

	  new = malloc (sizeof (char *) * i);
	  if (new)
	    {
	      if (umounts)
		snprintf (new, i, "%s:%s", umounts, SPLASH_CACHEDIR);
	      else
		snprintf (new, i, "%s", SPLASH_CACHEDIR);
	    }

	  /* We unsetenv first as some libc's leak memory if we overwrite
	     a var with a bigger value */
	  if (umounts)
	    unsetenv ("RC_NO_UMOUNTS");
	  setenv ("RC_NO_UMOUNTS", new, 1);

	  free (new);
	}
      return (_do_splash ("svc_stop", name, NULL));
    case rc_hook_service_stop_out:
      if (rc_service_state (name, rc_service_stopped))
	return (_do_splash ("svc_stopped", name, "0"));
      else
	return (_do_splash ("svc_started", name, "1"));
    case rc_hook_service_start_in:
      return (_do_splash ("svc_start", name, NULL));
    case rc_hook_service_start_out:
      if (rc_service_state (name, rc_service_stopped))
	return (_do_splash ("svc_started", name, "1"));
      else
	return (_do_splash ("svc_started", name, "0"));
    default: ;
    }

  return (0);
}
