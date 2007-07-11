/*
   librc-plugin.c 
   Simple plugin handler
   Copyright 2007 Gentoo Foundation
   Released under the GPLv2
   */

#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "einfo.h"
#include "rc.h"
#include "rc-misc.h"
#include "rc-plugin.h"
#include "strlist.h"

bool rc_in_plugin = false;

typedef struct plugin
{
	char *name;
	void *handle;
	int (*hook) (rc_hook_t, const char *);
	struct plugin *next;
} plugin_t;

static plugin_t *plugins = NULL;

#ifndef __FreeBSD__
dlfunc_t dlfunc (void * __restrict handle, const char * __restrict symbol)
{
	union {
		void *d;
		dlfunc_t f;
	} rv;

	rv.d = dlsym (handle, symbol);
	return (rv.f);
}
#endif

void rc_plugin_load (void)
{
	char **files;
	char *file;
	int i;
	plugin_t *plugin = plugins;

	/* Don't load plugins if we're in one */
	if (rc_in_plugin)
		return;

	/* Ensure some sanity here */
	rc_plugin_unload ();

	if (! rc_exists (RC_PLUGINDIR))
		return;

	files = rc_ls_dir (NULL, RC_PLUGINDIR, 0);
	STRLIST_FOREACH (files, file, i) {
		char *p = rc_strcatpaths (RC_PLUGINDIR, file, NULL);
		void *h = dlopen (p, RTLD_LAZY);
		char *func;
		int (*fptr) (rc_hook_t, const char *); 
		int len;

		if (! h) {
			eerror ("dlopen: %s", dlerror ());
			free (p);
			continue;
		}

		func = file;
		file = strsep (&func, ".");
		len = strlen (file) + 7;
		func = rc_xmalloc (sizeof (char *) * len);
		snprintf (func, len, "_%s_hook", file);

		fptr = (int (*)(rc_hook_t, const char*)) dlfunc (h, func);
		if (! fptr) {
			eerror ("`%s' does not expose the symbol `%s'", p, func);
			dlclose (h);
		} else {
			if (plugin) {
				plugin->next = rc_xmalloc (sizeof (plugin_t));
				plugin = plugin->next;
			} else
				plugin = plugins = rc_xmalloc (sizeof (plugin_t));

			memset (plugin, 0, sizeof (plugin_t));
			plugin->name = rc_xstrdup (file);
			plugin->handle = h;
			plugin->hook = fptr;
		}

		free (func);
		free (p);
	}

	rc_strlist_free (files);
}

void rc_plugin_run (rc_hook_t hook, const char *value)
{
	plugin_t *plugin = plugins;

	/* Don't run plugins if we're in one */
	if (rc_in_plugin)
		return;

	while (plugin) {
		if (plugin->hook) {
			int i;
			int flags;
			int pfd[2];
			pid_t pid;

			/* We create a pipe so that plugins can affect our environment
			 * vars, which in turn influence our scripts. */
			if (pipe (pfd) == -1) {
				eerror ("pipe: %s", strerror (errno));
				return;
			}

			/* Stop any scripts from inheriting us.
			 * This is actually quite important as without this, the splash
			 * plugin will probably hang when running in silent mode. */
			for (i = 0; i < 2; i++)
				if ((flags = fcntl (pfd[i], F_GETFD, 0)) < 0 ||
					fcntl (pfd[i], F_SETFD, flags | FD_CLOEXEC) < 0)
					eerror ("fcntl: %s", strerror (errno));

			/* We run the plugin in a new process so we never crash
			 * or otherwise affected by it */
			if ((pid = fork ()) == -1) {
				eerror ("fork: %s", strerror (errno));
				return;
			}

			if (pid == 0) {
				int retval;

				rc_in_plugin = true;
				close (pfd[0]);
				rc_environ_fd = fdopen (pfd[1], "w");
				retval = plugin->hook (hook, value);
				fclose (rc_environ_fd);
				rc_environ_fd = NULL;

				/* Just in case the plugin sets this to false */
				rc_in_plugin = true;
				exit (retval);
			} else {
				char buffer[RC_LINEBUFFER];
				char *token;
				char *p;
				ssize_t nr;

				close (pfd[1]);
				memset (buffer, 0, sizeof (buffer));

				while ((nr = read (pfd[0], buffer, sizeof (buffer))) > 0) {
					p = buffer;
					while (*p && p - buffer < nr) {
						token = strsep (&p, "=");
						if (token) {
							unsetenv (token);
							if (*p) {
								setenv (token, p, 1);
								p += strlen (p) + 1;
							} else
								p++;
						}
					}
				}

				close (pfd[0]);
			}
		}
		plugin = plugin->next;
	}
}

void rc_plugin_unload (void)
{
	plugin_t *plugin = plugins;
	plugin_t *next;

	while (plugin) {
		next = plugin->next;
		dlclose (plugin->handle);
		free (plugin->name);
		free (plugin);
		plugin = next;
	}
	plugins = NULL;
}
