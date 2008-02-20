/*
   librc-misc.c
   rc misc functions
   */

/*
 * Copyright 2007 Roy Marples
 * All rights reserved

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

#include <sys/types.h>

#ifdef __linux__
#include <sys/sysinfo.h>
#include <regex.h>
#endif

#include <sys/utsname.h>
#include <ctype.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "einfo.h"
#include "rc.h"
#include "rc-misc.h"
#include "strlist.h"

#define PROFILE_ENV     "/etc/profile.env"
#define SYS_WHITELIST   RC_LIBDIR "/conf.d/env_whitelist"
#define USR_WHITELIST   "/etc/conf.d/env_whitelist"
#define RC_CONF         "/etc/rc.conf"
#define RC_CONF_OLD     "/etc/conf.d/rc"

#define PATH_PREFIX     RC_LIBDIR "/bin:/bin:/sbin:/usr/bin:/usr/sbin"

static char **rc_conf = NULL;

static void _free_rc_conf (void)
{
	rc_strlist_free (rc_conf);
}

char *rc_conf_value (const char *setting)
{
	if (! rc_conf) {
		char *line;
		int i;

		rc_conf = rc_config_load (RC_CONF);
		atexit (_free_rc_conf);

		/* Support old configs */
		if (exists (RC_CONF_OLD)) {
			char **old = rc_config_load (RC_CONF_OLD);
			rc_strlist_join (&rc_conf, old);
			rc_strlist_free (old);
		}

		/* Convert old uppercase to lowercase */
		STRLIST_FOREACH (rc_conf, line, i) {
			char *p = line;
			while (p && *p && *p != '=') {
				if (isupper ((int) *p))
					*p = tolower ((int) *p);
				p++;
			}
		}
	}

	return (rc_config_value ((const char *const *)rc_conf, setting));
}

bool rc_conf_yesno (const char *setting)
{
	return (rc_yesno (rc_conf_value (setting)));
}

char **env_filter (void)
{
	char **env = NULL;
	char **whitelist = NULL;
	char *env_name = NULL;
	char **profile = NULL;
	int count = 0;
	bool got_path = false;
	char *env_var;
	size_t env_len;
	char *token;
	char *sep;
	char *e;
	char *p;
	size_t pplen = strlen (PATH_PREFIX);

	/* Init a system whitelist, start with shell vars we need */
	rc_strlist_add (&whitelist, "PATH");
	rc_strlist_add (&whitelist, "SHELL");
	rc_strlist_add (&whitelist, "USER");
	rc_strlist_add (&whitelist, "HOME");
	rc_strlist_add (&whitelist, "TERM");

	/* Add Language vars */
	rc_strlist_add (&whitelist, "LANG");
	rc_strlist_add (&whitelist, "LC_CTYPE");
	rc_strlist_add (&whitelist, "LC_NUMERIC");
	rc_strlist_add (&whitelist, "LC_TIME");
	rc_strlist_add (&whitelist, "LC_COLLATE");
	rc_strlist_add (&whitelist, "LC_MONETARY");
	rc_strlist_add (&whitelist, "LC_MESSAGES");
	rc_strlist_add (&whitelist, "LC_PAPER");
	rc_strlist_add (&whitelist, "LC_NAME");
	rc_strlist_add (&whitelist, "LC_ADDRESS");
	rc_strlist_add (&whitelist, "LC_TELEPHONE");
	rc_strlist_add (&whitelist, "LC_MEASUREMENT");
	rc_strlist_add (&whitelist, "LC_IDENTIFICATION");
	rc_strlist_add (&whitelist, "LC_ALL");

	/* Allow rc to override library path */
	rc_strlist_add (&whitelist, "LD_LIBRARY_PATH");

	/* We need to know sysvinit stuff - we emulate this for BSD too */
	rc_strlist_add (&whitelist, "INIT_HALT");
	rc_strlist_add (&whitelist, "INIT_VERSION");
	rc_strlist_add (&whitelist, "RUNLEVEL");
	rc_strlist_add (&whitelist, "PREVLEVEL");
	rc_strlist_add (&whitelist, "CONSOLE");

	/* Hotplug and daemon vars */
	rc_strlist_add (&whitelist, "IN_HOTPLUG");
	rc_strlist_add (&whitelist, "IN_BACKGROUND");
	rc_strlist_add (&whitelist, "RC_INTERFACE_KEEP_CONFIG");

	/* Add the user defined list of vars */
	e = env_name = xstrdup (rc_conf_value ("rc_env_allow"));
	while ((token = strsep (&e, " "))) {
		if (token[0] == '*') {
			free (env_name);
			return (NULL);
		}
		rc_strlist_add (&whitelist, token);
	}
	free (env_name);

	if (exists (PROFILE_ENV))
		profile = rc_config_load (PROFILE_ENV);

	STRLIST_FOREACH (whitelist, env_name, count) {
		char *space = strchr (env_name, ' ');
		if (space)
			*space = 0;

		env_var = getenv (env_name);

		if (! env_var && profile) {
			env_len = strlen (env_name) + strlen ("export ") + 1;
			p = xmalloc (sizeof (char) * env_len);
			snprintf (p, env_len, "export %s", env_name);
			env_var = rc_config_value ((const char *const *) profile, p);
			free (p);
		}

		if (! env_var)
			continue;

		/* Ensure our PATH is prefixed with the system locations first
		   for a little extra security */
		if (strcmp (env_name, "PATH") == 0 &&
		    strncmp (PATH_PREFIX, env_var, pplen) != 0)
		{
			got_path = true;
			env_len = strlen (env_name) + strlen (env_var) + pplen + 3;
			e = p = xmalloc (sizeof (char) * env_len);
			p += snprintf (e, env_len, "%s=%s", env_name, PATH_PREFIX);

			/* Now go through the env var and only add bits not in our PREFIX */
			sep = env_var;
			while ((token = strsep (&sep, ":"))) {
				char *np = xstrdup (PATH_PREFIX);
				char *npp = np;
				char *tok = NULL;
				while ((tok = strsep (&npp, ":")))
					if (strcmp (tok, token) == 0)
						break;
				if (! tok)
					p += snprintf (p, env_len - (p - e), ":%s", token);
				free (np);
			}
			*p++ = 0;
		} else {
			env_len = strlen (env_name) + strlen (env_var) + 2;
			e = xmalloc (sizeof (char) * env_len);
			snprintf (e, env_len, "%s=%s", env_name, env_var);
		}

		rc_strlist_add (&env, e);
		free (e);
	}

	/* We filtered the env but didn't get a PATH? Very odd.
	   However, we do need a path, so use a default. */
	if (! got_path) {
		env_len = strlen ("PATH=") + strlen (PATH_PREFIX) + 1;
		e = xmalloc (sizeof (char) * env_len);
		snprintf (e, env_len, "PATH=%s", PATH_PREFIX);
		rc_strlist_add (&env, e);
		free (e);
	}

	rc_strlist_free (whitelist);
	rc_strlist_free (profile);

	return (env);
}

char **env_config (void)
{
	char **env = NULL;
	char *line;
	size_t l;
	const char *sys = rc_sys ();
	struct utsname uts;
	FILE *fp;
	char buffer[PATH_MAX];
	char *runlevel = rc_runlevel_get ();

	/* One char less to drop the trailing / */
	l = strlen ("RC_LIBDIR=") + strlen (RC_LIBDIR) + 1;
	line = xmalloc (sizeof (char) * l);
	snprintf (line, l, "RC_LIBDIR=" RC_LIBDIR);
	rc_strlist_add (&env, line);
	free (line);

	/* One char less to drop the trailing / */
	l = strlen ("RC_SVCDIR=") + strlen (RC_SVCDIR) + 1;
	line = xmalloc (sizeof (char) * l);
	snprintf (line, l, "RC_SVCDIR=" RC_SVCDIR);
	rc_strlist_add (&env, line);
	free (line);

	rc_strlist_add (&env, "RC_BOOTLEVEL=" RC_LEVEL_BOOT);

	l = strlen ("RC_SOFTLEVEL=") + strlen (runlevel) + 1;
	line = xmalloc (sizeof (char) * l);
	snprintf (line, l, "RC_SOFTLEVEL=%s", runlevel);
	rc_strlist_add (&env, line);
	free (line);

	if ((fp = fopen (RC_KSOFTLEVEL, "r"))) {
		memset (buffer, 0, sizeof (buffer));
		if (fgets (buffer, sizeof (buffer), fp)) {
			l = strlen (buffer) - 1;
			if (buffer[l] == '\n')
				buffer[l] = 0;
			l += strlen ("RC_DEFAULTLEVEL=") + 2;
			line = xmalloc (sizeof (char) * l);
			snprintf (line, l, "RC_DEFAULTLEVEL=%s", buffer);
			rc_strlist_add (&env, line);
			free (line);
		}
		fclose (fp);
	} else
		rc_strlist_add (&env, "RC_DEFAULTLEVEL=" RC_LEVEL_DEFAULT);

	if (sys) {
		l = strlen ("RC_SYS=") + strlen (sys) + 2;
		line = xmalloc (sizeof (char) * l);
		snprintf (line, l, "RC_SYS=%s", sys);
		rc_strlist_add (&env, line);
		free (line);
	}

	/* Some scripts may need to take a different code path if Linux/FreeBSD, etc
	   To save on calling uname, we store it in an environment variable */
	if (uname (&uts) == 0) {
		l = strlen ("RC_UNAME=") + strlen (uts.sysname) + 2;
		line = xmalloc (sizeof (char) * l);
		snprintf (line, l, "RC_UNAME=%s", uts.sysname);
		rc_strlist_add (&env, line);
		free (line);
	}

	/* Be quiet or verbose as necessary */
	if (rc_conf_yesno ("rc_quiet"))
		rc_strlist_add (&env, "EINFO_QUIET=YES");
	if (rc_conf_yesno ("rc_verbose"))
		rc_strlist_add (&env, "EINFO_VERBOSE=YES");

	errno = 0;
	if ((! rc_conf_yesno ("rc_color") && errno == 0) ||
	    rc_conf_yesno ("rc_nocolor"))
		rc_strlist_add (&env, "EINFO_COLOR=NO");

	free (runlevel);
	return (env);
}

bool service_plugable (const char *service)
{
	char *list;
	char *p;
	char *star;
	char *token;
	bool allow = true;
	char *match = rc_conf_value ("rc_plug_services");

	if (! match)
		return (true);

	list = xstrdup (match);
	p = list;
	while ((token = strsep (&p, " "))) {
		bool truefalse = true;

		if (token[0] == '!') {
			truefalse = false;
			token++;
		}

		star = strchr (token, '*');
		if (star) {
			if (strncmp (service, token, (size_t) (star - token))
			    == 0)
			{
				allow = truefalse;
				break;
			}
		} else {
			if (strcmp (service, token) == 0) {
				allow = truefalse;
				break;
			}
		}
	}

	free (list);
	return (allow);
}

int signal_setup (int sig, void (*handler)(int))
{
	struct sigaction sa;

	memset (&sa, 0, sizeof (sa));
	sigemptyset (&sa.sa_mask);
	sa.sa_handler = handler;
	return (sigaction (sig, &sa, NULL));
}
