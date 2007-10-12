/*
   librc-misc.c
   rc misc functions
   Copyright 2007 Gentoo Foundation
   */

#include <sys/types.h>

#ifdef __linux__
#include <sys/sysinfo.h>
#include <regex.h>
#endif

#include <sys/utsname.h>
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
#define RC_CONFIG       "/etc/conf.d/rc"

#define PATH_PREFIX     RC_LIBDIR "/bin:/bin:/sbin:/usr/bin:/usr/sbin"

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
	char *p;
	char *token;
	char *sep;
	char *e;
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
	char *p;
	char **config;
	char *e;
#ifdef __linux__
	char sys[6];
#endif
	struct utsname uts;
	bool has_net_fs_list = false;
	FILE *fp;
	char buffer[PATH_MAX];
	char *runlevel = rc_runlevel_get ();

	/* Don't trust environ for softlevel yet */
	snprintf (buffer, PATH_MAX, "%s.%s", RC_CONFIG, runlevel);
	if (exists (buffer))
		config = rc_config_load (buffer);
	else
		config = rc_config_load (RC_CONFIG);

	STRLIST_FOREACH (config, line, i) {
		p = strchr (line, '=');
		if (! p)
			continue;

		*p = 0;
		e = getenv (line);
		if (! e) {
			*p = '=';
			rc_strlist_add (&env, line);
		} else {
			int len = strlen (line) + strlen (e) + 2;
			char *new = xmalloc (sizeof (char) * len);
			snprintf (new, len, "%s=%s", line, e);
			rc_strlist_add (&env, new);
			free (new);
		}
	}
	rc_strlist_free (config);

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

	/* Only add a NET_FS list if not defined */
	STRLIST_FOREACH (env, line, i)
		if (strncmp (line, "RC_NET_FS_LIST=", strlen ("RC_NET_FS_LIST=")) == 0) {
			has_net_fs_list = true;
			break;
		}

	if (! has_net_fs_list) {
		i = strlen ("RC_NET_FS_LIST=") + strlen (RC_NET_FS_LIST_DEFAULT) + 1;
		line = xmalloc (sizeof (char) * i);
		snprintf (line, i, "RC_NET_FS_LIST=%s", RC_NET_FS_LIST_DEFAULT);
		rc_strlist_add (&env, line);
		free (line);
	}

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
