/*
   librc-misc.c
   rc misc functions
   */

/*
 * Copyright 2007-2008 Roy Marples <roy@marples.name>
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
#include <sys/utsname.h>

#ifdef __linux__
#include <sys/sysinfo.h>
#include <regex.h>
#endif

#include <ctype.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "einfo.h"
#include "rc.h"
#include "rc-misc.h"

#define PROFILE_ENV     RC_SYSCONFDIR "/profile.env"
#define SYS_WHITELIST   RC_LIBDIR "/conf.d/env_whitelist"
#define USR_WHITELIST   RC_SYSCONFDIR "/conf.d/env_whitelist"
#define RC_CONF         RC_SYSCONFDIR "/rc.conf"
#define RC_CONF_OLD     RC_SYSCONFDIR "/conf.d/rc"

#define PATH_PREFIX     RC_LIBDIR "/bin:/bin:/sbin:/usr/bin:/usr/sbin"

static RC_STRINGLIST *rc_conf = NULL;

extern char** environ;

static void _free_rc_conf(void)
{
	rc_stringlist_free(rc_conf);
}

char *rc_conf_value(const char *setting)
{
	RC_STRINGLIST *old;
	RC_STRING *s;
	char *p;

	if (! rc_conf) {
		rc_conf = rc_config_load(RC_CONF);
		atexit(_free_rc_conf);

		/* Support old configs */
		if (exists(RC_CONF_OLD)) {
			old = rc_config_load(RC_CONF_OLD);
			if (old) {
				if (rc_conf) {
					TAILQ_CONCAT(rc_conf, old, entries);
					free(old);
				} else
					rc_conf = old;
			}
		}

		/* Convert old uppercase to lowercase */
		if (rc_conf)
			TAILQ_FOREACH(s, rc_conf, entries) {
				p = s->value;
				while (p && *p && *p != '=') {
					if (isupper((unsigned char)*p))
						*p = tolower((unsigned char)*p);
					p++;
				}
			}
	}

	return rc_config_value(rc_conf, setting);
}

bool rc_conf_yesno(const char *setting)
{
	return rc_yesno(rc_conf_value (setting));
}

static const char *const env_whitelist[] = {
	"PATH", "SHELL", "USER", "HOME", "TERM",
	"LANG", "LC_CTYPE", "LC_NUMERIC", "LC_TIME", "LC_COLLATE",
	"LC_MONETARY", "LC_MESSAGES", "LC_PAPER", "LC_NAME", "LC_ADDRESS",
	"LC_TELEPHONE", "LC_MEASUREMENT", "LC_IDENTIFICATION", "LC_ALL",
	"INIT_HALT", "INIT_VERSION", "RUNLEVEL", "PREVLEVEL", "CONSOLE",
	"IN_HOTPLUG", "IN_BACKGROUND", "RC_INTERFACE_KEEP_CONFIG",
	NULL
};

void env_filter(void)
{
	RC_STRINGLIST *env_allow;
	RC_STRINGLIST *profile = NULL;
	RC_STRINGLIST *env_list;
	RC_STRING *env;
	RC_STRING *s;
	char *env_name;
	char *e;
	char *token;
	size_t i = 0;

	/* Add the user defined list of vars */
	env_allow = rc_stringlist_new();
	e = env_name = xstrdup(rc_conf_value ("rc_env_allow"));
	while ((token = strsep(&e, " "))) {
		if (token[0] == '*') {
			free(env_name);
			rc_stringlist_free(env_allow);
			return;
		}
		rc_stringlist_add(env_allow, token);
	}
	free(env_name);

	if (exists(PROFILE_ENV))
		profile = rc_config_load(PROFILE_ENV);

	/* Copy the env and work from this so we can remove safely */
	env_list = rc_stringlist_new();
	while (environ[i])
		rc_stringlist_add(env_list, environ[i++]);

	TAILQ_FOREACH(env, env_list, entries) {
		/* Check the whitelist */
		i = 0;
		while (env_whitelist[i]) {
			if (strcmp(env_whitelist[i++], env->value))
				break;
		}
		if (env_whitelist[i])
			continue;

		/* Check our user defined list */
		TAILQ_FOREACH(s, env_allow, entries)
			if (strcmp(s->value, env->value) == 0)
				break;
		if (s)
			continue;

		/* Now check our profile */

		/* OK, not allowed! */
		e = strchr(env->value, '=');
		*e = '\0';
		unsetenv(env->value);
	}
	rc_stringlist_free(env_list);
	rc_stringlist_free(env_allow);
	rc_stringlist_free(profile);
}

void env_config(void)
{
	size_t pplen = strlen(PATH_PREFIX);
	char *path;
	char *p;
	char *e;
	size_t l;
	struct utsname uts;
	FILE *fp;
	char *token;
	char *np;
	char *npp;
	char *tok;
	const char *sys = rc_sys();
	char buffer[PATH_MAX];

	/* Ensure our PATH is prefixed with the system locations first
	   for a little extra security */
	path = getenv("PATH");
	if (! path)
		setenv("PATH", PATH_PREFIX, 1);
	else if (strncmp (PATH_PREFIX, path, pplen) != 0) {
		l = strlen(path) + pplen + 3;
		e = p = xmalloc(sizeof(char) * l);
		p += snprintf(p, l, "%s", PATH_PREFIX);

		/* Now go through the env var and only add bits not in our PREFIX */
		while ((token = strsep(&path, ":"))) {
			np = npp = xstrdup(PATH_PREFIX);
			while ((tok = strsep(&npp, ":")))
				if (strcmp(tok, token) == 0)
					break;
			if (! tok)
				p += snprintf(p, l - (p - e), ":%s", token);
			free (np);
		}
		*p++ = '\0';
		unsetenv("PATH");
		setenv("PATH", e, 1);
		free(e);
	}

	setenv("RC_LIBDIR", RC_LIBDIR, 1);
	setenv("RC_SVCDIR", RC_SVCDIR, 1);
	setenv("RC_BOOTLEVEL", RC_LEVEL_BOOT, 1);
	e = rc_runlevel_get();
	setenv("RC_RUNLEVEL", e, 1);
	free(e);

	if ((fp = fopen(RC_KRUNLEVEL, "r"))) {
		memset(buffer, 0, sizeof (buffer));
		if (fgets(buffer, sizeof (buffer), fp)) {
			l = strlen (buffer) - 1;
			if (buffer[l] == '\n')
				buffer[l] = 0;
			setenv("RC_DEFAULTLEVEL", buffer, 1);
		}
		fclose(fp);
	} else
		setenv("RC_DEFAULTLEVEL", RC_LEVEL_DEFAULT, 1);

	if (sys)
		setenv("RC_SYS", sys, 1);

	/* Some scripts may need to take a different code path if Linux/FreeBSD, etc
	   To save on calling uname, we store it in an environment variable */
	if (uname(&uts) == 0)
		setenv("RC_UNAME", uts.sysname, 1);

	/* Be quiet or verbose as necessary */
	if (rc_conf_yesno("rc_quiet"))
		setenv("EINFO_QUIET", "YES", 1);
	if (rc_conf_yesno("rc_verbose"))
		setenv("EINFO_VERBOSE", "YES", 1);

	errno = 0;
	if ((! rc_conf_yesno("rc_color") && errno == 0) ||
	    rc_conf_yesno("rc_nocolor"))
		setenv("EINFO_COLOR", "NO", 1);
}

bool service_plugable(const char *service)
{
	char *list;
	char *p;
	char *star;
	char *token;
	bool allow = true;
	char *match = rc_conf_value("rc_plug_services");
	bool truefalse;

	if (! match)
		return true;

	list = xstrdup(match);
	p = list;
	while ((token = strsep(&p, " "))) {
		if (token[0] == '!') {
			truefalse = false;
			token++;
		} else
			truefalse = true;

		star = strchr(token, '*');
		if (star) {
			if (strncmp(service, token, (size_t)(star - token)) == 0)
			{
				allow = truefalse;
				break;
			}
		} else {
			if (strcmp(service, token) == 0) {
				allow = truefalse;
				break;
			}
		}
	}

	free(list);
	return allow;
}

int signal_setup(int sig, void (*handler)(int))
{
	struct sigaction sa;

	memset(&sa, 0, sizeof (sa));
	sigemptyset(&sa.sa_mask);
	sa.sa_handler = handler;
	return sigaction(sig, &sa, NULL);
}

pid_t exec_service(const char *service, const char *arg)
{
	char *file;
	char fifo[PATH_MAX];
	pid_t pid = -1;
	sigset_t full;
	sigset_t old;
	struct sigaction sa;

	file = rc_service_resolve(service);
	if (! exists(file)) {
		rc_service_mark(service, RC_SERVICE_STOPPED);
		free(file);
		return 0;
	}

	/* We create a fifo so that other services can wait until we complete */
	snprintf(fifo, sizeof(fifo), RC_SVCDIR "/exclusive/%s",
		 basename_c(service));
	if (mkfifo(fifo, 0600) != 0 && errno != EEXIST) {
		free(file);
		return -1;
	}

	/* We need to block signals until we have forked */
	memset(&sa, 0, sizeof (sa));
	sa.sa_handler = SIG_DFL;
	sigemptyset(&sa.sa_mask);
	sigfillset(&full);
	sigprocmask(SIG_SETMASK, &full, &old);

	if ((pid = fork()) == 0) {
		/* Restore default handlers */
		sigaction(SIGCHLD, &sa, NULL);
		sigaction(SIGHUP, &sa, NULL);
		sigaction(SIGINT, &sa, NULL);
		sigaction(SIGQUIT, &sa, NULL);
		sigaction(SIGTERM, &sa, NULL);
		sigaction(SIGUSR1, &sa, NULL);
		sigaction(SIGWINCH, &sa, NULL);

		/* Unmask signals */
		sigprocmask(SIG_SETMASK, &old, NULL);

		/* Safe to run now */
		execl(file, file, arg, (char *) NULL);
		fprintf(stderr, "unable to exec `%s': %s\n",
			file, strerror(errno));
		unlink(fifo);
		_exit(EXIT_FAILURE);
	}

	if (pid == -1)
		fprintf(stderr, "fork: %s\n",strerror (errno));

	sigprocmask(SIG_SETMASK, &old, NULL);

	free(file);

	return pid;
}
