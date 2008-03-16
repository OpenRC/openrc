/*
   librc
   core RC functions
   */

/*
 * Copyright 2007-2008 Roy Marples
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

const char librc_copyright[] = "Copyright (c) 2007-2008 Roy Marples";

#include "librc.h"
#ifdef __FreeBSD__
#include <sys/sysctl.h>
#endif
#include <signal.h>

#define SOFTLEVEL	RC_SVCDIR "/softlevel"

#ifndef S_IXUGO
# define S_IXUGO (S_IXUSR | S_IXGRP | S_IXOTH)
#endif

/* File stream used for plugins to write environ vars to */
FILE *rc_environ_fd = NULL;

typedef struct rc_service_state_name {
	RC_SERVICE state;
	const char *name;
} rc_service_state_name_t;

/* We MUST list the states below 0x10 first
 * The rest can be in any order */
static const rc_service_state_name_t rc_service_state_names[] = {
	{ RC_SERVICE_STARTED,     "started" },
	{ RC_SERVICE_STOPPED,     "stopped" },
	{ RC_SERVICE_STARTING,    "starting" },
	{ RC_SERVICE_STOPPING,    "stopping" },
	{ RC_SERVICE_INACTIVE,    "inactive" },
	{ RC_SERVICE_WASINACTIVE, "wasinactive" },
	{ RC_SERVICE_COLDPLUGGED, "coldplugged" },
	{ RC_SERVICE_FAILED,      "failed" },
	{ RC_SERVICE_SCHEDULED,   "scheduled"},
	{ 0, NULL}
};

#define LS_INITD	0x01
#define LS_DIR   0x02
static RC_STRINGLIST *ls_dir(const char *dir, int options)
{
	DIR *dp;
	struct dirent *d;
	RC_STRINGLIST *list;
	struct stat buf;
	size_t l;
	char *file;
	int r;

	if ((dp = opendir(dir)) == NULL)
		return NULL;

	list = rc_stringlist_new();
	while (((d = readdir(dp)) != NULL)) {
		if (d->d_name[0] != '.') {
			if (options & LS_INITD) {
				/* Check that our file really exists.
				 * This is important as a service maybe in a runlevel, but
				 * could also have been removed. */
				file = rc_strcatpaths(dir, d->d_name, NULL);
				r = stat(file, &buf);
				free(file);
				if (r != 0)
					continue;

				/* .sh files are not init scripts */
				l = strlen(d->d_name);
				if (l > 2 && d->d_name[l - 3] == '.' &&
				    d->d_name[l - 2] == 's' &&
				    d->d_name[l - 1] == 'h')
					continue;
			}
			if (options & LS_DIR) {
				if (stat(d->d_name, &buf) == 0 &&
				    ! S_ISDIR(buf.st_mode))
					continue;
			}
			rc_stringlist_add(list, d->d_name);
		}
	}
	closedir(dp);

	return list;
}

static bool rm_dir(const char *pathname, bool top)
{
	DIR *dp;
	struct dirent *d;
	char *tmp = NULL;
	struct stat s;
	bool retval = true;

	if ((dp = opendir(pathname)) == NULL)
		return false;

	errno = 0;
	while (((d = readdir(dp)) != NULL) && errno == 0) {
		if (strcmp(d->d_name, ".") != 0 && strcmp(d->d_name, "..") != 0) {
			free(tmp);
			tmp = rc_strcatpaths(pathname, d->d_name, (char *) NULL);
			
			if (stat(tmp, &s) != 0) {
				retval = false;
				break;
			}

			if (S_ISDIR(s.st_mode)) {
				if (! rm_dir(tmp, true))
				{
					retval = false;
					break;
				}
			} else {
				if (unlink(tmp)) {
					retval = false;
					break;
				}
			}
		}
	}
	closedir(dp);
	free(tmp);

	if (! retval)
		return false;

	if (top && rmdir(pathname) != 0)
		return false;

	return true;
}

/* Other systems may need this at some point, but for now it's Linux only */
#ifdef __linux__
static bool file_regex(const char *file, const char *regex)
{
	FILE *fp;
	char *line;
	regex_t re;
	bool retval = false;
	int result;

	if (! (fp = fopen(file, "r")))
		return false;

	if ((result = regcomp(&re, regex, REG_EXTENDED | REG_NOSUB)) != 0) {
		fclose(fp);
		line = xmalloc(sizeof (char) * BUFSIZ);
		regerror(result, &re, line, BUFSIZ);
		fprintf(stderr, "file_regex: %s", line);
		free(line);
		return false;
	}

	while ((line = rc_getline(fp))) {
		if (regexec(&re, line, 0, NULL, 0) == 0)
			retval = true;
		free(line);
		if (retval)
			break;
	}
	fclose(fp);
	regfree(&re);

	return retval;
}
#endif


const char *rc_sys(void)
{
#ifdef PREFIX
	return RC_SYS_PREFIX;
#else	

#ifdef __FreeBSD__
	int jailed = 0;
	size_t len = sizeof(jailed);

	if (sysctlbyname("security.jail.jailed", &jailed, &len, NULL, 0) == 0)
		if (jailed == 1)
			return RC_SYS_JAIL;
#endif

#ifdef __linux__
	if (exists("/proc/xen")) {
		if (file_regex("/proc/xen/capabilities", "control_d"))
			return RC_SYS_XEN0;
		return RC_SYS_XENU;
	} else if (file_regex("/proc/cpuinfo", "UML"))
		return RC_SYS_UML;
	else if (file_regex("/proc/self/status",
			    "(s_context|VxID):[[:space:]]*[1-9]"))
		return RC_SYS_VSERVER;
	else if (file_regex("/proc/self/status",
			    "envID:[[:space:]]*[1-9]"))
		return RC_SYS_OPENVZ;
#endif

	return NULL;
#endif /* PREFIX */
}

static const char *rc_parse_service_state(RC_SERVICE state)
{
	int i;

	for (i = 0; rc_service_state_names[i].name; i++) {
		if (rc_service_state_names[i].state == state)
			return rc_service_state_names[i].name;
	}

	return NULL;
}

bool rc_runlevel_starting(void)
{
	return exists(RC_STARTING);
}
librc_hidden_def(rc_runlevel_starting)

bool rc_runlevel_stopping(void)
{
	return exists(RC_STOPPING);
}
librc_hidden_def(rc_runlevel_stopping)

RC_STRINGLIST *rc_runlevel_list(void)
{
	return ls_dir(RC_RUNLEVELDIR, LS_DIR);
}
librc_hidden_def(rc_runlevel_list)

char *rc_runlevel_get(void)
{
	FILE *fp;
	char *runlevel = NULL;

	if ((fp = fopen(SOFTLEVEL, "r"))) {
		runlevel = xmalloc(sizeof(char) * PATH_MAX);
		if (fgets(runlevel, PATH_MAX, fp)) {
			int i = strlen(runlevel) - 1;
			if (runlevel[i] == '\n')
				runlevel[i] = 0;
		} else
			*runlevel = '\0';
		fclose(fp);
	}

	if (! runlevel || ! *runlevel) {
		free(runlevel);
		runlevel = xstrdup(RC_LEVEL_SYSINIT);
	}

	return runlevel;
}
librc_hidden_def(rc_runlevel_get)

bool rc_runlevel_set(const char *runlevel)
{
	FILE *fp = fopen(SOFTLEVEL, "w");

	if (! fp)
		return false;
	fprintf(fp, "%s", runlevel);
	fclose(fp);
	return true;
}
librc_hidden_def(rc_runlevel_set)

bool rc_runlevel_exists(const char *runlevel)
{
	char *path;
	struct stat buf;
	bool retval = false;

	if (! runlevel)
		return false;

	path = rc_strcatpaths(RC_RUNLEVELDIR, runlevel, (char *) NULL);
	if (stat(path, &buf) == 0 && S_ISDIR(buf.st_mode))
		retval = true;
	free(path);
	return retval;
}
librc_hidden_def(rc_runlevel_exists)

/* Resolve a service name to it's full path */
char *rc_service_resolve(const char *service)
{
	char buffer[PATH_MAX];
	char *file;
	int r = 0;
	struct stat buf;

	if (! service)
		return NULL;

	if (service[0] == '/')
		return xstrdup(service);

	/* First check started services */
	file = rc_strcatpaths(RC_SVCDIR, "started", service, (char *) NULL);
	if (lstat(file, &buf) || ! S_ISLNK(buf.st_mode)) {
		free(file);
		file = rc_strcatpaths(RC_SVCDIR, "inactive", service, (char *) NULL);
		if (lstat(file, &buf) || ! S_ISLNK(buf.st_mode)) {
			free(file);
			file = NULL;
		}
	}

	memset(buffer, 0, sizeof(buffer));

	/* Nope, so lets see if the user has written it */
#ifdef RC_LOCAL_INITDIR
	snprintf(buffer, sizeof(buffer), RC_LOCAL_INITDIR "/%s", service);
	if (stat(buffer, &buf) == 0)
		return xstrdup(buffer);
#endif

	if (file) {
		r = readlink(file, buffer, sizeof(buffer));
		free(file);
		if (r > 0)
			return xstrdup(buffer);
	}
	snprintf(buffer, sizeof(buffer), RC_INITDIR "/%s", service);

	/* So we don't exist in /etc/init.d - check RC_PKG_INITDIR */
#ifdef RC_PKG_INITDIR
	if (stat(buffer, &buf) != 0) {
		snprintf(buffer, sizeof(buffer), RC_PKG_INITDIR "/%s", service);
		if (stat(buffer, &buf) != 0)
			return NULL;
	}
#endif

	return xstrdup(buffer);
}
librc_hidden_def(rc_service_resolve)

bool rc_service_exists(const char *service)
{
	char *file;
	bool retval = false;
	int len;
	struct stat buf;

	if (! service)
		return false;

	len = strlen(service);

	/* .sh files are not init scripts */
	if (len > 2 && service[len - 3] == '.' &&
	    service[len - 2] == 's' &&
	    service[len - 1] == 'h')
		return false;

	if (! (file = rc_service_resolve(service)))
		return false;

	if (stat(file, &buf) == 0 && buf.st_mode & S_IXUGO)
		retval = true;
	free(file);
	return retval;
}
librc_hidden_def(rc_service_exists)

#define OPTSTR ". '%s'; echo \"${opts}\""
RC_STRINGLIST *rc_service_extra_commands(const char *service)
{
	char *svc;
	char *cmd = NULL;
	char *buffer = NULL;
	RC_STRINGLIST *commands;
	char *token;
	char *p;
	FILE *fp;
	size_t l;

	if (! (svc = rc_service_resolve(service)))
		return NULL;

	commands = rc_stringlist_new();

	l = strlen(OPTSTR) + strlen(svc) + 1;
	cmd = xmalloc(sizeof(char) * l);
	snprintf(cmd, l, OPTSTR, svc);
	free(svc);

	if ((fp = popen(cmd, "r"))) {
		p = buffer = rc_getline(fp);
		while ((token = strsep(&p, " ")))
			rc_stringlist_add(commands, token);
		pclose(fp);
		free(buffer);
	}
	free(cmd);
	return commands;
}
librc_hidden_def(rc_service_extra_commands)

#define DESCSTR ". '%s'; echo \"${description%s%s}\""
char *rc_service_description(const char *service, const char *option)
{
	char *svc;
	char *cmd;
	char *desc = NULL;
	FILE *fp;
	size_t l;

	if (! (svc = rc_service_resolve(service)))
		return NULL;

	if (! option)
		option = "";

	l = strlen(DESCSTR) + strlen(svc) + strlen(option) + 2;
	cmd = xmalloc(sizeof(char) * l);
	snprintf(cmd, l, DESCSTR, svc, option ? "_" : "", option);
	free(svc);
	if ((fp = popen(cmd, "r"))) {
		desc = rc_getline(fp);
		pclose(fp);
	}
	free(cmd);
	return desc;
}
librc_hidden_def(rc_service_description)

bool rc_service_in_runlevel(const char *service, const char *runlevel)
{
	char *file;
	bool retval;

	if (! runlevel || ! service)
		return false;

	file = rc_strcatpaths(RC_RUNLEVELDIR, runlevel, basename_c(service),
			      (char *) NULL);
	retval = exists(file);
	free(file);

	return retval;
}
librc_hidden_def(rc_service_in_runlevel)

bool rc_service_mark(const char *service, const RC_SERVICE state)
{
	char *file;
	int i = 0;
	int skip_state = -1;
	const char *base;
	char *init = rc_service_resolve(service);
	bool skip_wasinactive = false;
	int s;
	char *was;
	RC_STRINGLIST *dirs;
	RC_STRING *dir;
	int serrno;

	if (! init)
		return false;

	base = basename_c(service);

	if (state != RC_SERVICE_STOPPED) {
		if (! exists(init)) {
			free(init);
			return false;
		}

		file = rc_strcatpaths(RC_SVCDIR, rc_parse_service_state (state), base,
				      (char *) NULL);
		if (exists(file))
			unlink(file);
		i = symlink(init, file);
		if (i != 0) {
			free(file);
			free(init);
			return false;
		}

		free(file);
		skip_state = state;
	}

	if (state == RC_SERVICE_COLDPLUGGED || state == RC_SERVICE_FAILED) {
		free(init);
		return true;
	}

	/* Remove any old states now */
	for (i = 0; rc_service_state_names[i].name; i++) {
		s = rc_service_state_names[i].state;

		if ((s != skip_state &&
		     s != RC_SERVICE_STOPPED &&
		     s != RC_SERVICE_COLDPLUGGED &&
		     s != RC_SERVICE_SCHEDULED) &&
		    (! skip_wasinactive || s != RC_SERVICE_WASINACTIVE))
		{
			file = rc_strcatpaths(RC_SVCDIR, rc_parse_service_state(s), base,
					      (char *) NULL);
			if (exists(file)) {
				if ((state == RC_SERVICE_STARTING ||
				     state == RC_SERVICE_STOPPING) &&
				    s == RC_SERVICE_INACTIVE)
				{
					was = rc_strcatpaths(RC_SVCDIR,
							     rc_parse_service_state(RC_SERVICE_WASINACTIVE),
							     base, (char *) NULL);

					symlink(init, was);
					skip_wasinactive = true;
					free(was);
				}
				unlink(file);
			}
			free(file);
		}
	}

	/* Remove the exclusive state if we're inactive */
	if (state == RC_SERVICE_STARTED ||
	    state == RC_SERVICE_STOPPED ||
	    state == RC_SERVICE_INACTIVE)
	{
		file = rc_strcatpaths(RC_SVCDIR, "exclusive", base, (char *) NULL);
		unlink(file);
		free(file);
	}

	/* Remove any options and daemons the service may have stored */
	if (state == RC_SERVICE_STOPPED) {
		file = rc_strcatpaths(RC_SVCDIR, "options", base, (char *) NULL);
		rm_dir(file, true);
		free(file);

		file = rc_strcatpaths(RC_SVCDIR, "daemons", base, (char *) NULL);
		rm_dir(file, true);
		free(file);

		rc_service_schedule_clear(service);
	}

	/* These are final states, so remove us from scheduled */
	if (state == RC_SERVICE_STARTED || state == RC_SERVICE_STOPPED) {
		file = rc_strcatpaths(RC_SVCDIR, "scheduled", (char *) NULL);
		dirs = ls_dir(file, 0);

		TAILQ_FOREACH(dir, dirs, entries) {
			was = rc_strcatpaths(file, dir->value, base, (char *) NULL);
			unlink(was);
			free(was);

			/* Try and remove the dir - we don't care about errors */
			was = rc_strcatpaths(file, dir->value, (char *) NULL);
			serrno = errno;
			rmdir(was);
			errno = serrno;
			free(was);
		}
		rc_stringlist_free(dirs);
	}

	free(init);
	return true;
}
librc_hidden_def(rc_service_mark)

RC_SERVICE rc_service_state(const char *service)
{
	int i;
	int state = RC_SERVICE_STOPPED;
	char *file;
	RC_STRINGLIST *dirs;
	RC_STRING *dir;

	for (i = 0; rc_service_state_names[i].name; i++) {
		file = rc_strcatpaths(RC_SVCDIR, rc_service_state_names[i].name,
				      basename_c(service), (char*) NULL);
		if (exists(file)) {
			if (rc_service_state_names[i].state <= 0x10)
				state = rc_service_state_names[i].state;
			else
				state |= rc_service_state_names[i].state;
		}
		free(file);
	}

	if (state & RC_SERVICE_STOPPED) {
		dirs = ls_dir(RC_SVCDIR "/scheduled", 0);
		TAILQ_FOREACH (dir, dirs, entries) {
			file = rc_strcatpaths(RC_SVCDIR, "scheduled",
					      dir->value,
					      service, (char *) NULL);
			if (exists(file))
				state |= RC_SERVICE_SCHEDULED;
			free(file);
			if (state & RC_SERVICE_SCHEDULED)
				break;
		}
		rc_stringlist_free(dirs);
	}

	return state;
}
librc_hidden_def(rc_service_state)

char *rc_service_value_get(const char *service, const char *option)
{
	FILE *fp;
	char *line = NULL;
	char *file = rc_strcatpaths(RC_SVCDIR, "options", service, option,
				    (char *) NULL);

	if ((fp = fopen(file, "r"))) {
		line = rc_getline(fp);
		fclose(fp);
	}
	free(file);

	return line;
}
librc_hidden_def(rc_service_value_get)

bool rc_service_value_set(const char *service, const char *option,
			  const char *value)
{
	FILE *fp;
	char *path = rc_strcatpaths(RC_SVCDIR, "options", service, (char *) NULL);
	char *file = rc_strcatpaths(path, option, (char *) NULL);
	bool retval = false;

	if (mkdir(path, 0755) != 0 && errno != EEXIST) {
		free(path);
		free(file);
		return false;
	}

	if ((fp = fopen(file, "w"))) {
		if (value)
			fprintf(fp, "%s", value);
		fclose(fp);
		retval = true;
	}

	free(path);
	free(file);
	return retval;
}
librc_hidden_def(rc_service_value_set)

static pid_t _exec_service(const char *service, const char *arg)
{
	char *file;
	char *fifo;
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
	fifo = rc_strcatpaths(RC_SVCDIR, "exclusive", basename_c(service),
			      (char *) NULL);

	if (mkfifo(fifo, 0600) != 0 && errno != EEXIST) {
		free(fifo);
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

	free(fifo);
	free(file);

	return pid;
}

pid_t rc_service_stop(const char *service)
{
	RC_SERVICE state = rc_service_state(service);

	if (state & RC_SERVICE_FAILED)
		return -1;

	if (state & RC_SERVICE_STOPPED)
		return 0;

	return _exec_service(service, "stop");
}
librc_hidden_def(rc_service_stop)

pid_t rc_service_start(const char *service)
{
	RC_SERVICE state = rc_service_state(service);

	if (state & RC_SERVICE_FAILED)
		return -1;

	if (! state & RC_SERVICE_STOPPED)
		return 0;

	return _exec_service(service, "start");
}
librc_hidden_def(rc_service_start)

bool rc_service_schedule_start(const char *service,
			       const char *service_to_start)
{
	char *dir;
	char *init;
	char *file;
	bool retval;

	/* service may be a provided service, like net */
	if (! service || ! rc_service_exists(service_to_start))
		return false;

	dir = rc_strcatpaths(RC_SVCDIR, "scheduled", basename_c(service),
			     (char *) NULL);
	if (mkdir(dir, 0755) != 0 && errno != EEXIST) {
		free(dir);
		return false;
	}

	init = rc_service_resolve(service_to_start);
	file = rc_strcatpaths(dir, basename_c(service_to_start), (char *) NULL);
	retval = (exists(file) || symlink(init, file) == 0);
	free(init);
	free(file);
	free(dir);

	return retval;
}
librc_hidden_def(rc_service_schedule_start)

bool rc_service_schedule_clear(const char *service)
{
	char *dir = rc_strcatpaths(RC_SVCDIR, "scheduled", basename_c(service),
				   (char *) NULL);
	bool retval;

	if (! (retval = rm_dir(dir, true)) && errno == ENOENT)
		retval = true;
	free(dir);
	return retval;
}
librc_hidden_def(rc_service_schedule_clear)

RC_STRINGLIST *rc_services_in_runlevel(const char *runlevel)
{
	char *dir;
	RC_STRINGLIST *list;

	if (! runlevel) {
#ifdef RC_PKG_INITDIR
		RC_STRINGLIST *pkg = ls_dir(RC_PKG_INITDIR, LS_INITD);
#endif
#ifdef RC_LOCAL_INITDIR
		RC_STRINGLIST *local = ls_dir(RC_LOCAL_INITDIR, LS_INITD);
#endif

		list = ls_dir(RC_INITDIR, LS_INITD);

#ifdef RC_PKG_INITDIR
		if (pkg) {
			TAILQ_CONCAT(list, pkg);
			free(pkg);
		}
#endif
#ifdef RC_LOCAL_DIR
		if (local) {
			TAILQ_CONCAT(list, local);
			free(local);
		}
#endif
		return list;
	}

	/* These special levels never contain any services */
	if (strcmp(runlevel, RC_LEVEL_SYSINIT) == 0 ||
	    strcmp(runlevel, RC_LEVEL_SINGLE) == 0) {
		list = rc_stringlist_new();
		return list;
	}

	dir = rc_strcatpaths(RC_RUNLEVELDIR, runlevel, (char *) NULL);
	list = ls_dir(dir, LS_INITD);
	free(dir);
	return list;
}
librc_hidden_def(rc_services_in_runlevel)

RC_STRINGLIST *rc_services_in_state(RC_SERVICE state)
{
	char *dir = rc_strcatpaths(RC_SVCDIR, rc_parse_service_state(state),
				   (char *) NULL);
	RC_STRINGLIST *services;
	RC_STRINGLIST *list;
	RC_STRINGLIST *dirs;
	RC_STRING *d;
	char *p;

	if (state == RC_SERVICE_SCHEDULED) {
		dirs = ls_dir(dir, 0);
		list = rc_stringlist_new();
		TAILQ_FOREACH(d, dirs, entries) {
			p = rc_strcatpaths(dir, d->value, (char *) NULL);
			services = ls_dir(p, LS_INITD);
			free(p);
			TAILQ_CONCAT(list, services);
			free(services);
		}
		rc_stringlist_free(dirs);
	} else {
		list = ls_dir(dir, LS_INITD);
	}

	free(dir);
	return list;
}
librc_hidden_def(rc_services_in_state)

bool rc_service_add(const char *runlevel, const char *service)
{
	bool retval;
	char *init;
	char *file;
	char path[MAXPATHLEN] = { '\0' };
	char *p;

	if (! rc_runlevel_exists(runlevel)) {
		errno = ENOENT;
		return false;
	}

	if (rc_service_in_runlevel(service, runlevel)) {
		errno = EEXIST;
		return false;
	}

	init = rc_service_resolve(service);

	/* We need to ensure that only things in /etc/init.d are added
	 * to the boot runlevel */
	if (strcmp (runlevel, RC_LEVEL_BOOT) == 0) {
		p = realpath(dirname (init), path);
		free(init);
		if (! *p)
			return false;

		retval = (strcmp(path, RC_INITDIR) == 0);
		if (! retval) {
			errno = EPERM;
			return false;
		}
		init = rc_strcatpaths(RC_INITDIR, service, (char *) NULL); 
	}

	file = rc_strcatpaths(RC_RUNLEVELDIR, runlevel, basename_c(service),
			      (char *) NULL);
	retval = (symlink(init, file) == 0);
	free(init);
	free(file);
	return retval;
}
librc_hidden_def(rc_service_add)

bool rc_service_delete (const char *runlevel, const char *service)
{
	char *file;
	bool retval = false;

	if (! runlevel || ! service)
		return false;

	file = rc_strcatpaths (RC_RUNLEVELDIR, runlevel, basename_c(service),
			       (char *) NULL);
	if (unlink(file) == 0)
		retval = true;

	free(file);
	return retval;
}
librc_hidden_def(rc_service_delete)

RC_STRINGLIST *rc_services_scheduled_by(const char *service)
{
	RC_STRINGLIST *dirs = ls_dir(RC_SVCDIR "/scheduled", 0);
	RC_STRINGLIST *list;
	RC_STRING *dir;
	char *file;

	list = rc_stringlist_new();
	TAILQ_FOREACH (dir, dirs, entries) {
		file = rc_strcatpaths(RC_SVCDIR, "scheduled", dir->value,
				      service, (char *) NULL);
		if (exists(file))
			rc_stringlist_add(list, file);
		free(file);
	}
	rc_stringlist_free(dirs);

	return list;
}
librc_hidden_def(rc_services_scheduled_by)

RC_STRINGLIST *rc_services_scheduled(const char *service)
{
	char *dir = rc_strcatpaths(RC_SVCDIR, "scheduled", basename_c(service),
				   (char *) NULL);
	RC_STRINGLIST *list = ls_dir(dir, LS_INITD);

	free(dir);
	return list;
}
librc_hidden_def(rc_services_scheduled)
