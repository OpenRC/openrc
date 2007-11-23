/*
   librc-misc.c
   rc misc functions
   */

/* 
 * Copyright 2007 Gentoo Foundation
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

bool rc_conf_yesno (const char *setting)
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
				if (isupper (*p))
					*p = tolower (*p);
				p++;
			}
		}
	
	}

	return (rc_yesno (rc_config_value (rc_conf, setting)));
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
	int env_len;
	char *token;
	char *sep;
	char *e;
	char *p;
	int pplen = strlen (PATH_PREFIX);

	whitelist = rc_config_list (SYS_WHITELIST);
	if (! whitelist)
		fprintf (stderr, "system environment whitelist (" SYS_WHITELIST ") missing\n");

	env = rc_config_list (USR_WHITELIST);
	rc_strlist_join (&whitelist, env);
	rc_strlist_free (env);
	env = NULL;

	if (! whitelist)
		return (NULL);

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
			env_var = rc_config_value (profile, p);
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
			env_len = strlen (env_name) + strlen (env_var) + pplen + 2;
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
		env_len = strlen ("PATH=") + strlen (PATH_PREFIX) + 2;
		p = xmalloc (sizeof (char) * env_len);
		snprintf (p, env_len, "PATH=%s", PATH_PREFIX);
		rc_strlist_add (&env, p);
		free (p);
	}

	rc_strlist_free (whitelist);
	rc_strlist_free (profile);

	return (env);
}

	/* Other systems may need this at some point, but for now it's Linux only */
#ifdef __linux__
static bool file_regex (const char *file, const char *regex)
{
	FILE *fp;
	char *buffer;
	regex_t re;
	bool retval = false;
	int result;

	if (! (fp = fopen (file, "r")))
		return (false);

	buffer = xmalloc (sizeof (char) * RC_LINEBUFFER);
	if ((result = regcomp (&re, regex, REG_EXTENDED | REG_NOSUB)) != 0) {
		fclose (fp);
		regerror (result, &re, buffer, RC_LINEBUFFER);
		fprintf (stderr, "file_regex: %s", buffer);
		free (buffer);
		return (false);
	}

	while (fgets (buffer, RC_LINEBUFFER, fp)) {
		if (regexec (&re, buffer, 0, NULL, 0) == 0)
		{
			retval = true;
			break;
		}
	}
	free (buffer);
	fclose (fp);
	regfree (&re);

	return (retval);
}
#endif

char **env_config (void)
{
	char **env = NULL;
	char *line;
	int i;
#ifdef __linux__
	char sys[6];
#endif
	struct utsname uts;
	FILE *fp;
	char buffer[PATH_MAX];
	char *runlevel = rc_runlevel_get ();

	/* One char less to drop the trailing / */
	i = strlen ("RC_LIBDIR=") + strlen (RC_LIBDIR) + 1;
	line = xmalloc (sizeof (char) * i);
	snprintf (line, i, "RC_LIBDIR=" RC_LIBDIR);
	rc_strlist_add (&env, line);
	free (line);

	/* One char less to drop the trailing / */
	i = strlen ("RC_SVCDIR=") + strlen (RC_SVCDIR) + 1;
	line = xmalloc (sizeof (char) * i);
	snprintf (line, i, "RC_SVCDIR=" RC_SVCDIR);
	rc_strlist_add (&env, line);
	free (line);

	rc_strlist_add (&env, "RC_BOOTLEVEL=" RC_LEVEL_BOOT);

	i = strlen ("RC_SOFTLEVEL=") + strlen (runlevel) + 1;
	line = xmalloc (sizeof (char) * i);
	snprintf (line, i, "RC_SOFTLEVEL=%s", runlevel);
	rc_strlist_add (&env, line);
	free (line);

	if ((fp = fopen (RC_KSOFTLEVEL, "r"))) {
		memset (buffer, 0, sizeof (buffer));
		if (fgets (buffer, sizeof (buffer), fp)) {
			i = strlen (buffer) - 1;
			if (buffer[i] == '\n')
				buffer[i] = 0;
			i += strlen ("RC_DEFAULTLEVEL=") + 2;
			line = xmalloc (sizeof (char) * i);
			snprintf (line, i, "RC_DEFAULTLEVEL=%s", buffer);
			rc_strlist_add (&env, line);
			free (line);
		}
		fclose (fp);
	} else
		rc_strlist_add (&env, "RC_DEFAULTLEVEL=" RC_LEVEL_DEFAULT);


#ifdef __linux__
	/* Linux can run some funky stuff like Xen, VServer, UML, etc
	   We store this special system in RC_SYS so our scripts run fast */
	memset (sys, 0, sizeof (sys));

	if (exists ("/proc/xen")) {
		if ((fp = fopen ("/proc/xen/capabilities", "r"))) {
			fclose (fp);
			if (file_regex ("/proc/xen/capabilities", "control_d"))
				snprintf (sys, sizeof (sys), "XENU");
		}
		if (! sys[0])
			snprintf (sys, sizeof (sys), "XEN0");
	} else if (file_regex ("/proc/cpuinfo", "UML")) {
		snprintf (sys, sizeof (sys), "UML");
	} else if (file_regex ("/proc/self/status",
						   "(s_context|VxID|envID):[[:space:]]*[1-9]"))
	{
		snprintf (sys, sizeof (sys), "VPS");
	}

	if (sys[0]) {
		i = strlen ("RC_SYS=") + strlen (sys) + 2;
		line = xmalloc (sizeof (char) * i);
		snprintf (line, i, "RC_SYS=%s", sys);
		rc_strlist_add (&env, line);
		free (line);
	}

#endif

	/* Some scripts may need to take a different code path if Linux/FreeBSD, etc
	   To save on calling uname, we store it in an environment variable */
	if (uname (&uts) == 0) {
		i = strlen ("RC_UNAME=") + strlen (uts.sysname) + 2;
		line = xmalloc (sizeof (char) * i);
		snprintf (line, i, "RC_UNAME=%s", uts.sysname);
		rc_strlist_add (&env, line);
		free (line);
	}

	free (runlevel);
	return (env);
}
