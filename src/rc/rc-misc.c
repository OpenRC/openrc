/*
  librc-misc.c
  rc misc functions
*/

/*
 * Copyright (c) 2007-2008 Roy Marples <roy@marples.name>
 *
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

#include <sys/file.h>
#include <sys/types.h>
#include <sys/utsname.h>

#ifdef __linux__
#  include <sys/sysinfo.h>
#  include <regex.h>
#endif

#include <ctype.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "einfo.h"
#include "queue.h"
#include "rc.h"
#include "rc-misc.h"
#include "version.h"

extern char **environ;

bool
rc_conf_yesno(const char *setting)
{
	return rc_yesno(rc_conf_value (setting));
}

static const char *const env_whitelist[] = {
	"CONSOLE", "PATH", "SHELL", "USER", "HOME", "TERM",
	"LANG", "LC_CTYPE", "LC_NUMERIC", "LC_TIME", "LC_COLLATE",
	"LC_MONETARY", "LC_MESSAGES", "LC_PAPER", "LC_NAME", "LC_ADDRESS",
	"LC_TELEPHONE", "LC_MEASUREMENT", "LC_IDENTIFICATION", "LC_ALL",
	"IN_HOTPLUG", "IN_BACKGROUND", "RC_INTERFACE_KEEP_CONFIG",
	NULL
};

void
env_filter(void)
{
	RC_STRINGLIST *env_allow;
	RC_STRINGLIST *profile;
	RC_STRINGLIST *env_list;
	RC_STRING *env;
	char *e;
	size_t i = 0;

	/* Add the user defined list of vars */
	env_allow = rc_stringlist_split(rc_conf_value("rc_env_allow"), " ");
	profile = rc_config_load(RC_PROFILE_ENV);

	/* Copy the env and work from this so we can manipulate it safely */
	env_list = rc_stringlist_new();
	while (environ && environ[i]) {
		env = rc_stringlist_add(env_list, environ[i++]);
		e = strchr(env->value, '=');
		if (e)
			*e = '\0';
	}

	TAILQ_FOREACH(env, env_list, entries) {
		/* Check the whitelist */
		for (i = 0; env_whitelist[i]; i++) {
			if (strcmp(env_whitelist[i], env->value) == 0)
				break;
		}
		if (env_whitelist[i])
			continue;

		/* Check our user defined list */
		if (rc_stringlist_find(env_allow, env->value))
			continue;

		/* OK, not allowed! */
		unsetenv(env->value);
	}

	/* Now add anything missing from the profile */
	TAILQ_FOREACH(env, profile, entries) {
		e = strchr(env->value, '=');
		*e = '\0';
		if (!getenv(env->value))
			setenv(env->value, e + 1, 1);
	}

#ifdef DEBUG_MEMORY
	rc_stringlist_free(env_list);
	rc_stringlist_free(env_allow);
	rc_stringlist_free(profile);
#endif
}

void
env_config(void)
{
	size_t pplen = strlen(RC_PATH_PREFIX);
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
		setenv("PATH", RC_PATH_PREFIX, 1);
	else if (strncmp (RC_PATH_PREFIX, path, pplen) != 0) {
		l = strlen(path) + pplen + 3;
		e = p = xmalloc(sizeof(char) * l);
		p += snprintf(p, l, "%s", RC_PATH_PREFIX);

		/* Now go through the env var and only add bits not in our
		 * PREFIX */
		while ((token = strsep(&path, ":"))) {
			np = npp = xstrdup(RC_PATH_PREFIX);
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

	setenv("RC_VERSION", VERSION, 1);
	setenv("RC_LIBEXECDIR", RC_LIBEXECDIR, 1);
	setenv("RC_SVCDIR", RC_SVCDIR, 1);
	setenv("RC_TMPDIR", RC_SVCDIR "/tmp", 1);
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

#ifdef PREFIX
	setenv("RC_PREFIX", RC_PREFIX, 1);
#endif

	/* Some scripts may need to take a different code path if
	   Linux/FreeBSD, etc
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

int
signal_setup(int sig, void (*handler)(int))
{
	struct sigaction sa;

	memset(&sa, 0, sizeof (sa));
	sigemptyset(&sa.sa_mask);
	sa.sa_handler = handler;
	return sigaction(sig, &sa, NULL);
}

int
svc_lock(const char *applet)
{
	char file[PATH_MAX];
	int fd;

	snprintf(file, sizeof(file), RC_SVCDIR "/exclusive/%s", applet);
	fd = open(file, O_WRONLY | O_CREAT | O_NONBLOCK, 0664);
	if (fd == -1)
		return -1;
	if (flock(fd, LOCK_EX | LOCK_NB) == -1) {
		close(fd);
		return -1;
	}
	return fd;
}

int
svc_unlock(const char *applet, int fd)
{
	char file[PATH_MAX];

	snprintf(file, sizeof(file), RC_SVCDIR "/exclusive/%s", applet);
	close(fd);
	unlink(file);
	return -1;
}

pid_t
exec_service(const char *service, const char *arg)
{
	char *file, sfd[32];
	int fd;
	pid_t pid = -1;
	sigset_t full;
	sigset_t old;
	struct sigaction sa;

	fd = svc_lock(basename_c(service));
	if (fd == -1)
		return -1;

	file = rc_service_resolve(service);
	if (!exists(file)) {
		rc_service_mark(service, RC_SERVICE_STOPPED);
		svc_unlock(basename_c(service), fd);
		free(file);
		return 0;
	}
	snprintf(sfd, sizeof(sfd), "%d", fd);

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
		execl(file, file, "--lockfd", sfd, arg, (char *) NULL);
		fprintf(stderr, "unable to exec `%s': %s\n",
		    file, strerror(errno));
		svc_unlock(basename_c(service), fd);
		_exit(EXIT_FAILURE);
	}

	if (pid == -1) {
		fprintf(stderr, "fork: %s\n",strerror (errno));
		svc_unlock(basename_c(service), fd);
	} else
		fcntl(fd, F_SETFD, fcntl(fd, F_GETFD, 0) | FD_CLOEXEC);

	sigprocmask(SIG_SETMASK, &old, NULL);
	free(file);
	return pid;
}

int
parse_mode(mode_t *mode, char *text)
{
	char *p;
	unsigned long l;

	/* Check for a numeric mode */
	if ((*text - '0') < 8) {
		l = strtoul(text, &p, 8);
		if (*p || l > 07777U) {
			errno = EINVAL;
			return -1;
		}
		*mode = (mode_t) l;
		return 0;
	}

	/* We currently don't check g+w type stuff */
	errno = EINVAL;
	return -1;
}

int
is_writable(const char *path)
{
	if (access(path, W_OK) == 0)
		return 1;

	return 0;
}
