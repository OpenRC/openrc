/*
 * rc-misc.c
 * rc misc functions
 */

/*
 * Copyright (c) 2007-2015 The OpenRC Authors.
 * See the Authors file at the top-level directory of this distribution and
 * https://github.com/OpenRC/openrc/blob/master/AUTHORS
 *
 * This file is part of OpenRC. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution and at https://github.com/OpenRC/openrc/blob/master/LICENSE
 * This file may not be copied, modified, propagated, or distributed
 *    except according to the terms contained in the LICENSE file.
 */

#include <sys/file.h>
#include <sys/types.h>
#include <sys/utsname.h>

#ifdef __linux__
#  include <sys/sysinfo.h>
#endif

#include <ctype.h>
#include <fcntl.h>
#include <limits.h>
#  include <regex.h>
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
	"EERROR_QUIET", "EINFO_QUIET",
	"IN_BACKGROUND", "IN_HOTPLUG",
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
	char *sys = NULL;
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

	sys = detect_container();
	if (!sys)
		sys = detect_vm();
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

static bool file_regex(const char *file, const char *regex)
{
	FILE *fp;
	char *line = NULL;
	size_t len = 0;
	regex_t re;
	bool retval = true;
	int result;

	if (!(fp = fopen(file, "r")))
		return false;

	if ((result = regcomp(&re, regex, REG_EXTENDED | REG_NOSUB)) != 0) {
		fclose(fp);
		line = xmalloc(sizeof(char) * BUFSIZ);
		regerror(result, &re, line, BUFSIZ);
		fprintf(stderr, "file_regex: %s", line);
		free(line);
		return false;
	}

	while ((rc_getline(&line, &len, fp))) {
		char *str = line;
		/* some /proc files have \0 separated content so we have to
		   loop through the 'line' */
		do {
			if (regexec(&re, str, 0, NULL, 0) == 0)
				goto found;
			str += strlen(str) + 1;
			/* len is the size of allocated buffer and we don't
			   want call regexec BUFSIZE times. find next str */
			while (str < line + len && *str == '\0')
				str++;
		} while (str < line + len);
	}
	retval = false;
found:
	fclose(fp);
	free(line);
	regfree(&re);

	return retval;
}

char *detect_prefix(void)
{
#ifdef PREFIX
	return RC_SYS_PREFIX;
#else
	return NULL;
#endif
}

char *get_systype(void)
{
	char *systype = rc_conf_value("rc_sys");
	if (systype) {
		char *s = systype;
		/* Convert to uppercase */
		while (s && *s) {
			if (islower((unsigned char) *s))
				*s = toupper((unsigned char) *s);
			s++;
		}
	}
	return systype;
}

char *detect_container(void)
{
	char *systype = get_systype();

#ifdef __FreeBSD__
	if (systype && strcmp(systype, RC_SYS_JAIL) == 0)
		return RC_SYS_JAIL;
	int jailed = 0;
	size_t len = sizeof(jailed);

	if (sysctlbyname("security.jail.jailed", &jailed, &len, NULL, 0) == 0)
		if (jailed == 1)
			return RC_SYS_JAIL;
#endif

#ifdef __linux__
	if (systype) {
		if (strcmp(systype, RC_SYS_UML) == 0)
			return RC_SYS_UML;
		if (strcmp(systype, RC_SYS_VSERVER) == 0)
			return RC_SYS_VSERVER;
		if (strcmp(systype, RC_SYS_OPENVZ) == 0)
			return RC_SYS_OPENVZ;
		if (strcmp(systype, RC_SYS_LXC) == 0)
			return RC_SYS_LXC;
		if (strcmp(systype, RC_SYS_RKT) == 0)
				return RC_SYS_RKT;
		if (strcmp(systype, RC_SYS_SYSTEMD_NSPAWN) == 0)
				return RC_SYS_SYSTEMD_NSPAWN;
		if (strcmp(systype, RC_SYS_DOCKER) == 0)
				return RC_SYS_DOCKER;
	}
	if (file_regex("/proc/cpuinfo", "UML"))
		return RC_SYS_UML;
	else if (file_regex("/proc/self/status",
		"(s_context|VxID):[[:space:]]*[1-9]"))
		return RC_SYS_VSERVER;
	else if (exists("/proc/vz/veinfo") && !exists("/proc/vz/version"))
		return RC_SYS_OPENVZ;
	else if (file_regex("/proc/self/status",
		"envID:[[:space:]]*[1-9]"))
		return RC_SYS_OPENVZ; /* old test */
	else if (file_regex("/proc/1/environ", "container=lxc"))
		return RC_SYS_LXC;
	else if (file_regex("/proc/1/environ", "container=rkt"))
		return RC_SYS_RKT;
	else if (file_regex("/proc/1/environ", "container=systemd-nspawn"))
		return RC_SYS_SYSTEMD_NSPAWN;
	else if (file_regex("/proc/1/environ", "container=docker"))
		return RC_SYS_DOCKER;
#endif

	return NULL;
}

char *detect_vm(void)
{
	char *systype = get_systype();

#ifdef __NetBSD__
	if (systype) {
		if(strcmp(systype, RC_SYS_XEN0) == 0)
				return RC_SYS_XEN0;
		if (strcmp(systype, RC_SYS_XENU) == 0)
			return RC_SYS_XENU;
	}
	if (exists("/kern/xen/privcmd"))
		return RC_SYS_XEN0;
	if (exists("/kern/xen"))
		return RC_SYS_XENU;
#endif

#ifdef __linux__
	if (systype) {
		if (strcmp(systype, RC_SYS_XEN0) == 0)
			return RC_SYS_XEN0;
		if (strcmp(systype, RC_SYS_XENU) == 0)
			return RC_SYS_XENU;
	}
	if (exists("/proc/xen")) {
		if (file_regex("/proc/xen/capabilities", "control_d"))
			return RC_SYS_XEN0;
		return RC_SYS_XENU;
	}
#endif

	return NULL;
}
