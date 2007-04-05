/*
   librc-plugin.c 
   Simple plugin handler
   Copyright 2007 Gentoo Foundation
   Released under the GPLv2
   */

#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "einfo.h"
#include "rc.h"
#include "rc-misc.h"
#include "rc-plugin.h"
#include "strlist.h"

typedef struct plugin
{
  char *name;
  void *handle;
  int (*hook) (rc_hook_t hook, const char *name);
  struct plugin *next;
} plugin_t;

static plugin_t *plugins = NULL;

void rc_plugin_load (void)
{
  char **files;
  char *file;
  int i;
  plugin_t *plugin = plugins;

  /* Ensure some sanity here */
  rc_plugin_unload ();

  if (! rc_exists (RC_PLUGINDIR))
    return;

  files = rc_ls_dir (NULL, RC_PLUGINDIR, 0);
  STRLIST_FOREACH (files, file, i)
    {
      char *p = rc_strcatpaths (RC_PLUGINDIR, file, NULL);
      void *h = dlopen (p, RTLD_LAZY);
      char *func;
      void *f;

      if (! h)
	{
	  eerror ("dlopen `%s': %s", p, dlerror ());
	  free (p);
	  continue;
	}

      func = file;
      file = strsep (&func, ".");
      func = rc_xmalloc (strlen (file) + strlen ("__hook") + 1);
      sprintf (func, "_%s_hook", file);

      f = dlsym (h, func);
      if (! f)
	{
	  eerror ("`%s' does not expose the symbol `%s'", p, func);
	  dlclose (h);
	}
      else
	{
	  if (plugin)
	    {
	      plugin->next = rc_xmalloc (sizeof (plugin_t));
	      plugin = plugin->next;
	    }
	  else
	    plugin = plugins = rc_xmalloc (sizeof (plugin_t));

	  memset (plugin, 0, sizeof (plugin_t));
	  plugin->name = strdup (file);
	  plugin->handle = h;
	  plugin->hook = f;
	}

      free (func);
      free (p);
    }

  rc_strlist_free (files);
}

void rc_plugin_run (rc_hook_t hook, const char *value)
{
  plugin_t *plugin = plugins;

  while (plugin)
    {
      if (plugin->hook)
	plugin->hook (hook, value);

      plugin = plugin->next;
    }
}

void rc_plugin_unload (void)
{
  plugin_t *plugin = plugins;
  plugin_t *next;

  while (plugin)
    {
      next = plugin->next;
      dlclose (plugin->handle);
      free (plugin->name);
      free (plugin);
      plugin = next;
    }
  plugins = NULL;
}
