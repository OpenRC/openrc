/*
 * rc.c
 * rc - manager for init scripts which control the startup, shutdown
 * and the running of daemons.
 *
 * Also a multicall binary for various commands that can be used in shell
 * scripts to query service state, mark service state and provide the
 * einfo family of informational functions.
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

const char rc_copyright[] = "Copyright (c) 2007-2008 Roy Marples";

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <sys/wait.h>

/* So we can coldplug net devices */
#ifdef BSD
# include <sys/socket.h>
# include <ifaddrs.h>
#endif

#ifdef __linux__
# include <asm/setup.h> /* for COMMAND_LINE_SIZE */
#endif

#include <errno.h>
#include <dirent.h>
#include <ctype.h>
#include <getopt.h>
#include <libgen.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <strings.h>
#include <termios.h>
#include <unistd.h>

#include "builtins.h"
#include "einfo.h"
#include "rc.h"
#include "rc-logger.h"
#include "rc-misc.h"
#include "rc-plugin.h"

#include "version.h"

#define INITSH                  RC_LIBDIR "/sh/init.sh"
#define INITEARLYSH             RC_LIBDIR "/sh/init-early.sh"
#define HALTSH                  RC_INITDIR "/halt.sh"

#define SHUTDOWN                "/sbin/shutdown"
#define SULOGIN                 "/sbin/sulogin"

#define INTERACTIVE             RC_SVCDIR "/interactive"

#define DEVBOOT			"/dev/.rcboot"

static char *RUNLEVEL = NULL;
static char *PREVLEVEL = NULL;

const char *applet = NULL;
static char *runlevel = NULL;
static RC_STRINGLIST *coldplugged_services = NULL;
static RC_STRINGLIST *stop_services = NULL;
static RC_STRINGLIST *start_services = NULL;
static RC_STRINGLIST *types_n = NULL;
static RC_STRINGLIST *types_nua = NULL;
static RC_DEPTREE *deptree = NULL;
static RC_HOOK hook_out = 0;

struct termios *termios_orig = NULL;

RC_PIDLIST service_pids;

static void clean_failed(void)
{
	DIR *dp;
	struct dirent *d;
	size_t l;
	char *path;

	/* Clean the failed services state dir now */
	if ((dp = opendir(RC_SVCDIR "/failed"))) {
		while ((d = readdir(dp))) {
			if (d->d_name[0] == '.' &&
			    (d->d_name[1] == '\0' ||
			     (d->d_name[1] == '.' && d->d_name[2] == '\0')))
				continue;

			l = strlen(RC_SVCDIR "/failed/") + strlen(d->d_name) + 1;
			path = xmalloc(sizeof(char) * l);
			snprintf(path, l, RC_SVCDIR "/failed/%s", d->d_name);
			if (path) {
				if (unlink(path))
					eerror("%s: unlink `%s': %s", applet, path,
					       strerror(errno));
				free(path);
			}
		}
		closedir(dp);
	}
}

static void cleanup(void)
{
	if (applet && strcmp(applet, "rc") == 0) {
		RC_PID *p1 = LIST_FIRST(&service_pids);
		RC_PID *p2;

		if (hook_out)
			rc_plugin_run(hook_out, runlevel);

		rc_plugin_unload();

		if (! rc_in_plugin && termios_orig) {
			tcsetattr(fileno(stdin), TCSANOW, termios_orig);
			free(termios_orig);
		}

		while (p1) {
			p2 = LIST_NEXT(p1, entries); 
			free(p1);
			p1 = p2;
		}

		rc_stringlist_free(coldplugged_services);
		rc_stringlist_free(stop_services);
		rc_stringlist_free(start_services);
		rc_stringlist_free(types_n);
		rc_stringlist_free(types_nua);
		rc_deptree_free(deptree);

		/* Clean runlevel start, stop markers */
		if (! rc_in_plugin && ! rc_in_logger) {
			rmdir(RC_STARTING);
			rmdir(RC_STOPPING);
			clean_failed();

			rc_logger_close();
		}

		free(runlevel);
	}
}

#ifdef __linux__
static char *proc_getent(const char *ent)
{
	FILE *fp;
	char proc[COMMAND_LINE_SIZE];
	char *p;
	char *value = NULL;
	int i;

	if (! exists("/proc/cmdline"))
		return NULL;

	if (! (fp = fopen("/proc/cmdline", "r"))) {
		eerror("failed to open `/proc/cmdline': %s", strerror(errno));
		return NULL;
	}

	memset(proc, 0, sizeof(proc));
	fgets(proc, sizeof(proc), fp);
	if (*proc && (p = strstr(proc, ent))) {
		i = p - proc;
		if (i == '\0' || proc[i - 1] == ' ') {
			p += strlen(ent);
			if (*p == '=')
				p++;
			value = xstrdup(strsep(&p, " "));
		}
	} else
		errno = ENOENT;
	fclose(fp);

	return value;
}
#endif

static char read_key(bool block)
{
	struct termios termios;
	char c = 0;
	int fd = fileno(stdin);

	if (! isatty(fd))
		return false;

	/* Now save our terminal settings. We need to restore them at exit as we
	 * will be changing it for non-blocking reads for Interactive */
	if (! termios_orig) {
		termios_orig = xmalloc(sizeof(*termios_orig));
		tcgetattr(fd, termios_orig);
	}

	tcgetattr(fd, &termios);
	termios.c_lflag &= ~(ICANON | ECHO);
	if (block)
		termios.c_cc[VMIN] = 1;
	else {
		termios.c_cc[VMIN] = 0;
		termios.c_cc[VTIME] = 0;
	}
	tcsetattr(fd, TCSANOW, &termios);

	read(fd, &c, 1);

	tcsetattr(fd, TCSANOW, termios_orig);

	return c;
}

static bool want_interactive(void)
{
	char c;
	static bool gotinteractive;
	static bool interactive;

	if (rc_yesno(getenv("EINFO_QUIET")))
		return false;

	if (PREVLEVEL &&
	    strcmp(PREVLEVEL, "N") != 0 &&
	    strcmp(PREVLEVEL, "S") != 0 &&
	    strcmp(PREVLEVEL, "1") != 0)
		return false;

	if (! gotinteractive) {
		gotinteractive = true;
		interactive = rc_conf_yesno("rc_interactive");
	}
	if (! interactive)
		return false;

	c = read_key(false);
	return (c == 'I' || c == 'i') ? true : false;
}

static void mark_interactive(void)
{
	FILE *fp = fopen(INTERACTIVE, "w");
	if (fp)
		fclose(fp);
}

static void sulogin(bool cont)
{
	int status = 0;
	struct sigaction sa;
	sigset_t full;
	sigset_t old;
	pid_t pid;
#ifdef __linux__
	const char *sys = rc_sys();

	/* VSERVER and OPENVZ systems cannot do a sulogin */
	if (sys && (strcmp(sys, "VSERVER") == 0 || strcmp(sys, "OPENVZ") == 0)) {
		execl("/sbin/halt", "/sbin/halt", "-f", (char *) NULL);
		eerrorx("%s: unable to exec `/sbin/halt': %s",
			applet, strerror(errno));
	}
#endif

	if (! cont) {
		rc_logger_close();
#ifdef __linux__
		execl("/sbin/sulogin", "/sbin/sulogin", (char *) NULL);
		eerrorx("%s: unable to exec `/sbin/sulogin': %s",
			applet, strerror(errno));
#else
		exit(EXIT_SUCCESS);
#endif
	}

	/* We need to block signals until we have forked */
	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = SIG_DFL;
	sigemptyset(&sa.sa_mask);
	sigfillset(&full);
	sigprocmask(SIG_SETMASK, &full, &old);
	pid = vfork();

	if (pid == -1)
		eerrorx("%s: fork: %s", applet, strerror(errno));
	if (pid == 0) {
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

		if (termios_orig)
			tcsetattr(fileno(stdin), TCSANOW, termios_orig);

#ifdef __linux__
		execl(SULOGIN, SULOGIN, (char *) NULL);
		eerror("%s: unable to exec `%s': %s", applet, SULOGIN,
		       strerror(errno));
#else
		execl("/bin/sh", "/bin/sh", (char *) NULL);
		eerror("%s: unable to exec `/bin/sh': %s", applet,
		       strerror(errno));
#endif
		_exit(EXIT_FAILURE);
	}

	/* Unmask signals and wait for child */
	sigprocmask(SIG_SETMASK, &old, NULL);
	waitpid(pid, &status, 0);
}

static void single_user(void)
{
	rc_logger_close();

	execl(SHUTDOWN, SHUTDOWN, "now", (char *) NULL);
	eerrorx("%s: unable to exec `" SHUTDOWN "': %s",
		 applet, strerror(errno));
}

static bool set_ksoftlevel(const char *level)
{
	FILE *fp;

	if (! level ||
	    strcmp(level, getenv ("RC_BOOTLEVEL")) == 0 ||
	    strcmp(level, RC_LEVEL_SINGLE) == 0 ||
	    strcmp(level, RC_LEVEL_SYSINIT) == 0)
	{
		if (exists(RC_KSOFTLEVEL) &&
		    unlink(RC_KSOFTLEVEL) != 0)
			eerror("unlink `%s': %s", RC_KSOFTLEVEL, strerror(errno));
		return false;
	}

	if (! (fp = fopen(RC_KSOFTLEVEL, "w"))) {
		eerror("fopen `%s': %s", RC_KSOFTLEVEL, strerror(errno));
		return false;
	}

	fprintf(fp, "%s", level);
	fclose(fp);
	return true;
}

static int get_ksoftlevel(char *buffer, int buffer_len)
{
	FILE *fp;
	int i = 0;

	if (! exists(RC_KSOFTLEVEL))
		return 0;

	if (! (fp = fopen(RC_KSOFTLEVEL, "r"))) {
		eerror("fopen `%s': %s", RC_KSOFTLEVEL, strerror(errno));
		return -1;
	}

	if (fgets(buffer, buffer_len, fp)) {
		i = strlen(buffer) - 1;
		if (buffer[i] == '\n')
			buffer[i] = 0;
	}

	fclose(fp);
	return i;
}

static void add_pid(pid_t pid)
{
	RC_PID *p = xmalloc(sizeof(*p));
	p->pid = pid;
	LIST_INSERT_HEAD(&service_pids, p, entries);
}

static void remove_pid(pid_t pid)
{
	RC_PID *p;

	LIST_FOREACH(p, &service_pids, entries)
		if (p->pid == pid) {
			LIST_REMOVE(p, entries);
			free(p);
			return;
		}
}

static void wait_for_services(void)
{
	while (waitpid(0, 0, 0) != -1);
}

static void handle_signal(int sig)
{
	int serrno = errno;
	char signame[10] = { '\0' };
	pid_t pid;
	RC_PID *pi;
	int status = 0;
	struct winsize ws;
	sigset_t sset;

	switch (sig) {
	case SIGCHLD:
		do {
			pid = waitpid(-1, &status, WNOHANG);
			if (pid < 0) {
				if (errno != ECHILD)
					eerror("waitpid: %s", strerror(errno));
				return;
			}
		} while (! WIFEXITED(status) && ! WIFSIGNALED(status));

		/* Remove that pid from our list */
		if (pid > 0)
			remove_pid(pid);
		break;

	case SIGWINCH:
		if (rc_logger_tty >= 0) {
			ioctl(STDIN_FILENO, TIOCGWINSZ, &ws);
			ioctl(rc_logger_tty, TIOCSWINSZ, &ws);
		}
		break;

	case SIGINT:
		if (! signame[0])
			snprintf(signame, sizeof(signame), "SIGINT");
		/* FALLTHROUGH */
	case SIGTERM:
		if (! signame[0])
			snprintf(signame, sizeof(signame), "SIGTERM");
		/* FALLTHROUGH */
	case SIGQUIT:
		if (! signame[0])
			snprintf(signame, sizeof(signame), "SIGQUIT");
		eerrorx("%s: caught %s, aborting", applet, signame);
		/* NOTREACHED */
	case SIGUSR1:
		eerror("rc: Aborting!");

		/* Block child signals */
		sigemptyset(&sset);
		sigaddset(&sset, SIGCHLD);
		sigprocmask(SIG_BLOCK, &sset, NULL);

		/* Kill any running services we have started */
		LIST_FOREACH(pi, &service_pids, entries)
			kill(pi->pid, SIGTERM);

		/* Notify plugins we are aborting */
		rc_plugin_run(RC_HOOK_ABORT, NULL);

		/* Only drop into single user mode if we're booting */
		if ((PREVLEVEL &&
		     (strcmp(PREVLEVEL, "S") == 0 ||
		      strcmp(PREVLEVEL, "1") == 0)) ||
		    (RUNLEVEL &&
		     (strcmp(RUNLEVEL, "S") == 0 ||
		      strcmp(RUNLEVEL, "1") == 0)))
			single_user();

		exit(EXIT_FAILURE);
		/* NOTREACHED */

	default:
		eerror("%s: caught unknown signal %d", applet, sig);
	}

	/* Restore errno */
	errno = serrno;
}

static void run_script(const char *script)
{
	int status = 0;
	pid_t pid = vfork();
	pid_t wpid;

	if (pid < 0)
		eerrorx("%s: vfork: %s", applet, strerror(errno));
	else if (pid == 0) {
		execl(script, script, (char *) NULL);
		eerror("%s: unable to exec `%s': %s",
			script, applet, strerror(errno));
		_exit(EXIT_FAILURE);
	}

	do {
		wpid = waitpid(pid, &status, 0);
		if (wpid < 1)
			eerror("waitpid: %s", strerror(errno));
	} while (! WIFEXITED(status) && ! WIFSIGNALED(status));

	if (! WIFEXITED(status) || ! WEXITSTATUS(status) == 0)
		eerrorx("%s: failed to exec `%s'", applet, script);
}

static void do_coldplug(void)
{
	size_t l;
	DIR *dp;
	struct dirent *d;
	char *service;
	RC_STRING *s;
#ifdef BSD
	struct ifaddrs *ifap;
	struct ifaddrs *ifa;
	char *p;
#endif

	if (! rc_conf_yesno("rc_coldplug") && errno != ENOENT)
		return;

	/* We need to ensure our state dirs exist.
	 * We should have a better call than this, but oh well. */
	rc_deptree_update_needed();

#ifdef BSD
	if (getifaddrs(&ifap) == 0) {
		for (ifa = ifap; ifa; ifa = ifa->ifa_next) {
			if (ifa->ifa_addr->sa_family != AF_LINK)
				continue;

			l = strlen("net.") + strlen(ifa->ifa_name) + 1;
			service = xmalloc(sizeof (char) * l);
			snprintf(service, l, "net.%s", ifa->ifa_name);
			if (rc_service_exists(service) &&
			    service_plugable(service))
				rc_service_mark(service, RC_SERVICE_COLDPLUGGED);
			free(service);
		}
		freeifaddrs (ifap);
	}

	/* The mice are a little more tricky.
	 * If we coldplug anything else, we'll probably do it here. */
	if ((dp = opendir("/dev"))) {
		while ((d = readdir(dp))) {
			if (strncmp(d->d_name, "psm", 3) == 0 ||
			    strncmp(d->d_name, "ums", 3) == 0)
			{
				p = d->d_name + 3;
				if (p && isdigit((int) *p)) {
					l = strlen("moused.") + strlen(d->d_name) + 1;
					service = xmalloc(sizeof(char) * l);
					snprintf (service, l, "moused.%s", d->d_name);
					if (rc_service_exists (service) &&
					    service_plugable (service))
						rc_service_mark (service, RC_SERVICE_COLDPLUGGED);
					free(service);
				}
			}
		}
		closedir (dp);
	}

#else

	/* udev likes to start services before we're ready when it does
	 * its coldplugging thing. runscript knows when we're not ready so it
	 * stores a list of coldplugged services in DEVBOOT for us to pick up
	 * here when we are ready for them */
	if ((dp = opendir(DEVBOOT))) {
		while ((d = readdir(dp))) {
			if (d->d_name[0] == '.' &&
			    (d->d_name[1] == '\0' ||
			     (d->d_name[1] == '.' && d->d_name[2] == '\0')))
				continue;

			if (rc_service_exists(d->d_name) &&
			    service_plugable(d->d_name))
				rc_service_mark(d->d_name, RC_SERVICE_COLDPLUGGED);

			l = strlen(DEVBOOT "/") + strlen(d->d_name) + 1;
			service = xmalloc(sizeof (char) * l);
			snprintf(service, l, DEVBOOT "/%s", d->d_name);
			if (unlink(service))
				eerror("%s: unlink `%s': %s", applet, service,
					strerror(errno));
			free(service);
		}
		closedir(dp);
		rmdir(DEVBOOT);
	}
#endif

	if (rc_yesno(getenv("EINFO_QUIET")))
		return;

	/* Load our list of coldplugged services and display them */
	einfon("Device initiated services:%s", ecolor(ECOLOR_HILITE));
	coldplugged_services = rc_services_in_state(RC_SERVICE_COLDPLUGGED);
	TAILQ_FOREACH(s, coldplugged_services, entries)
		printf(" %s", s->value);
	printf ("%s\n", ecolor(ECOLOR_NORMAL));
}

static void do_newlevel(const char *newlevel)
{
	struct utsname uts;
	const char *sys;
#ifdef __linux__
	char *cmd;
#endif

	if (strcmp(newlevel, RC_LEVEL_SYSINIT) == 0
#ifndef PREFIX
	    && RUNLEVEL &&
	    (strcmp(RUNLEVEL, "S") == 0 ||
	     strcmp(RUNLEVEL, "1") == 0)
#endif
	   )
	{
		/* OK, we're either in runlevel 1 or single user mode */

		/* exec init-early.sh if it exists
		 * This should just setup the console to use the correct
		 * font. Maybe it should setup the keyboard too? */
		if (exists(INITEARLYSH))
			run_script(INITEARLYSH);

		uname(&uts);
		printf("\n   %sOpenRC %s" VERSION "%s is starting up %s",
		       ecolor(ECOLOR_GOOD), ecolor(ECOLOR_HILITE),
		       ecolor(ECOLOR_NORMAL), ecolor(ECOLOR_BRACKET));
#ifdef BRANDING
		printf(BRANDING " (%s)", uts.machine);
#else
		printf("%s %s (%s)",
		       uts.sysname,
		       uts.release,
		       uts.machine);
#endif

		if ((sys = rc_sys()))
			printf(" [%s]", sys);

		printf("%s\n\n", ecolor(ECOLOR_NORMAL));

		if (! rc_yesno(getenv ("EINFO_QUIET")) &&
		    rc_conf_yesno("rc_interactive"))
			printf("Press %sI%s to enter interactive boot mode\n\n",
			       ecolor(ECOLOR_GOOD), ecolor(ECOLOR_NORMAL));

		setenv("RC_SOFTLEVEL", newlevel, 1);
		rc_plugin_run(RC_HOOK_RUNLEVEL_START_IN, newlevel);
		hook_out = RC_HOOK_RUNLEVEL_START_OUT;
		run_script(INITSH);

#ifdef __linux__
		/* If we requested a softlevel, save it now */
		set_ksoftlevel(NULL);
		if ((cmd = proc_getent("softlevel"))) {
			set_ksoftlevel(cmd);
			free(cmd);
		}
#endif

		/* Setup our coldplugged services now */
		do_coldplug();

		rc_plugin_run(RC_HOOK_RUNLEVEL_START_OUT, newlevel);
		hook_out = 0;

		if (want_interactive())
			mark_interactive();

		exit(EXIT_SUCCESS);
	} else if (strcmp(newlevel, RC_LEVEL_SINGLE) == 0) {
#ifndef PREFIX
		if (! RUNLEVEL ||
		    (strcmp(RUNLEVEL, "S") != 0 &&
		     strcmp(RUNLEVEL, "1") != 0))
		{
			/* Remember the current runlevel for when we come back */
			set_ksoftlevel(runlevel);
			single_user();
		}
#endif
	} else if (strcmp(newlevel, RC_LEVEL_REBOOT) == 0) {
		if (! RUNLEVEL ||
		    strcmp(RUNLEVEL, "6") != 0)
		{
			rc_logger_close();
			execl(SHUTDOWN, SHUTDOWN, "-r", "now", (char *) NULL);
			eerrorx("%s: unable to exec `" SHUTDOWN "': %s",
				applet, strerror(errno));
		}
	} else if (strcmp(newlevel, RC_LEVEL_SHUTDOWN) == 0) {
		if (! RUNLEVEL ||
		    strcmp(RUNLEVEL, "0") != 0)
		{
			rc_logger_close();
			execl(SHUTDOWN, SHUTDOWN,
#ifdef __linux__
			      "-h",
#else
			      "-p",
#endif
			      "now", (char *) NULL);
			eerrorx("%s: unable to exec `" SHUTDOWN "': %s",
				applet, strerror(errno));
		}
	}
}


static bool runlevel_config(const char *service, const char *level)
{
	char *init = rc_service_resolve(service);
	char *conf;
	size_t l;
	bool retval;

	init = dirname(init);
	init = dirname(init);
	l = strlen(init) + strlen(level) + strlen(service) + 10;
	conf = xmalloc(sizeof(char) * l);
	snprintf(conf, l, "%s/conf.d/%s.%s", init, service, level);
	retval = exists(conf);
	free(conf);

	return retval;
}

static void do_stop_services(const char *newlevel, bool going_down, bool parallel)
{
	pid_t pid;
	RC_STRING *service, *svc1, *svc2;
	RC_STRINGLIST *deporder, *tmplist;

	if (! types_n) {
		types_n = rc_stringlist_new();
		rc_stringlist_add(types_n, "needsme");
	}

	TAILQ_FOREACH_REVERSE(service, stop_services, rc_stringlist, entries)
	{
		if (rc_service_state(service->value) & RC_SERVICE_STOPPED)
			continue;

		/* We always stop the service when in these runlevels */
		if (going_down || ! start_services) {
			pid = rc_service_stop(service->value);
			if (pid > 0 && ! parallel)
				rc_waitpid(pid);
			continue;
		}

		/* If we're in the start list then don't bother stopping us */
		TAILQ_FOREACH(svc1, start_services, entries)
			if (strcmp (svc1->value, service->value) == 0)
				break;

		if (svc1) {
			if (newlevel && strcmp(runlevel, newlevel) != 0) {
				/* So we're in the start list. But we should
				 * be stopped if we have a runlevel
				 * configuration file for either the current
				 * or next so we use the correct one. */
				if (! runlevel_config(service->value, runlevel) &&
				    ! runlevel_config(service->value, newlevel))
					continue;
			}
			else
				continue;
		}

		/* We got this far! Or last check is to see if any any service
		 * that going to be started depends on us */
		if (! svc1) {
			tmplist = rc_stringlist_new();
			rc_stringlist_add(tmplist, service->value);
			deporder = rc_deptree_depends(deptree, types_n, tmplist,
						      runlevel, RC_DEP_STRICT);
			rc_stringlist_free(tmplist);
			svc2 = NULL;
			TAILQ_FOREACH (svc1, deporder, entries) {
				TAILQ_FOREACH(svc2, start_services, entries)
					if (strcmp (svc1->value, svc2->value) == 0)
						break;
				if (svc2)
					break;
			}
			rc_stringlist_free(deporder);

			if (svc2)
				continue;
		}

		/* After all that we can finally stop the blighter! */
		pid = rc_service_stop(service->value);
		if (pid > 0) {
			add_pid(pid);
			if (! parallel) {
				rc_waitpid(pid);
				remove_pid(pid);
			}
		}
	}
}

static void do_start_services(bool parallel)
{
	RC_STRING *service;
	pid_t pid;
	bool interactive = false;

	if (! rc_yesno(getenv("EINFO_QUIET")))
		interactive = exists(INTERACTIVE);

	TAILQ_FOREACH(service, start_services, entries) {
		if (rc_service_state(service->value) & RC_SERVICE_STOPPED) {
			if (! interactive)
				interactive = want_interactive();

			if (interactive) {
interactive_retry:
				printf("\n");
				einfo("About to start the service %s",
				      service->value);
				eindent();
				einfo("1) Start the service\t\t2) Skip the service");
				einfo("3) Continue boot process\t\t4) Exit to shell");
				eoutdent();
interactive_option:
				switch (read_key(true)) {
				case '1': break;
				case '2': continue;
				case '3': interactive = false; break;
				case '4': sulogin(true); goto interactive_retry;
				default: goto interactive_option;
				}
			}

			pid = rc_service_start(service->value);
			
			/* Remember the pid if we're running in parallel */
			if (pid > 0) {
				add_pid(pid);

				if (! parallel) {
					rc_waitpid(pid);
					remove_pid(pid);
				}
			}
		}
	}

	/* Store our interactive status for boot */
	if (interactive && strcmp(runlevel, getenv("RC_BOOTLEVEL")) == 0)
		mark_interactive();
	else {
		if (exists(INTERACTIVE))
			unlink(INTERACTIVE);
	}

}

#include "_usage.h"
#define getoptstring "o:" getoptstring_COMMON
static const struct option longopts[] = {
	{ "override", 1, NULL, 'o' },
	longopts_COMMON
};
static const char * const longopts_help[] = {
	"override the next runlevel to change into\nwhen leaving single user or boot runlevels",
	longopts_help_COMMON
};
#include "_usage.c"

int main(int argc, char **argv)
{
	const char *bootlevel = NULL;
	char *newlevel = NULL;
	RC_STRINGLIST *deporder = NULL;
	RC_STRINGLIST *tmplist;
	RC_STRING *service;
	bool going_down = false;
	int depoptions = RC_DEP_STRICT | RC_DEP_TRACE;
	char ksoftbuffer [PATH_MAX];
	char pidstr[6];
	int opt;
	bool parallel;
	int regen = 0;
#ifdef __linux__
	char *proc;
	char *p;
	char *token;
#endif

	applet = basename_c(argv[0]);
	LIST_INIT(&service_pids);
	atexit(cleanup);
	if (! applet)
		eerrorx("arguments required");

	if (argc > 1 && (strcmp(argv[1], "--version") == 0)) {
		printf("%s (OpenRC", applet);
		if ((bootlevel = rc_sys()))
			printf(" [%s]", bootlevel);
		printf(") " VERSION
#ifdef BRANDING
		       " (" BRANDING ")"
#endif
		       "\n");
		exit(EXIT_SUCCESS);
	}

	/* Run our built in applets. If we ran one, we don't return. */
	run_applets(argc, argv);

	argc--;
	argv++;

	/* Change dir to / to ensure all scripts don't use stuff in pwd */
	chdir("/");

	/* RUNLEVEL is set by sysvinit as is a magic number
	 * RC_SOFTLEVEL is set by us and is the name for this magic number
	 * even though all our userland documentation refers to runlevel */
	RUNLEVEL = getenv("RUNLEVEL");
	PREVLEVEL = getenv("PREVLEVEL");

	/* Ensure our environment is pure
	 * Also, add our configuration to it */
	env_filter();
	env_config();

	argc++;
	argv--;
	while ((opt = getopt_long(argc, argv, getoptstring,
				  longopts, (int *) 0)) != -1)
	{
		switch (opt) {
		case 'o':
			if (*optarg == '\0')
				optarg = NULL;
			exit(set_ksoftlevel(optarg) ? EXIT_SUCCESS : EXIT_FAILURE);
			/* NOTREACHED */
		case_RC_COMMON_GETOPT
		}
	}

	newlevel = argv[optind++];

	/* Enable logging */
	setenv("EINFO_LOG", "rc", 1);

	/* Export our PID */
	snprintf(pidstr, sizeof(pidstr), "%d", getpid());
	setenv("RC_PID", pidstr, 1);

	/* Load current softlevel */
	bootlevel = getenv("RC_BOOTLEVEL");
	runlevel = rc_runlevel_get();

	rc_logger_open(newlevel ? newlevel : runlevel);

	/* Setup a signal handler */
	signal_setup(SIGINT, handle_signal);
	signal_setup(SIGQUIT, handle_signal);
	signal_setup(SIGTERM, handle_signal);
	signal_setup(SIGUSR1, handle_signal);
	signal_setup(SIGWINCH, handle_signal);

	rc_plugin_load();

	/* Check we're in the runlevel requested, ie from
	 * rc single
	 * rc shutdown
	 * rc reboot
	 */
	if (newlevel)
		do_newlevel(newlevel);

	/* Now we start handling our children */
	signal_setup(SIGCHLD, handle_signal);

	/* We should only use ksoftlevel if we were in single user mode
	 * If not, we need to erase ksoftlevel now. */
	if (PREVLEVEL &&
	    (strcmp(PREVLEVEL, "1") == 0 ||
	     strcmp(PREVLEVEL, "S") == 0 ||
	     strcmp(PREVLEVEL, "N") == 0))
	{
		/* Try not to join boot and ksoftlevels together */
		if (! newlevel ||
		    strcmp(newlevel, getenv("RC_BOOTLEVEL")) != 0)
			if (get_ksoftlevel(ksoftbuffer, sizeof(ksoftbuffer)))
				newlevel = ksoftbuffer;
	} else if (! RUNLEVEL ||
		   (strcmp(RUNLEVEL, "1") != 0 &&
		    strcmp(RUNLEVEL, "S") != 0 &&
		    strcmp(RUNLEVEL, "N") != 0))
	{
		set_ksoftlevel(NULL);
	}

	if (newlevel &&
	    (strcmp(newlevel, RC_LEVEL_REBOOT) == 0 ||
	     strcmp(newlevel, RC_LEVEL_SHUTDOWN) == 0 ||
	     strcmp(newlevel, RC_LEVEL_SINGLE) == 0))
	{
		going_down = true;
		rc_runlevel_set(newlevel);
		setenv("RC_RUNLEVEL", newlevel, 1);

#ifdef __FreeBSD__
		/* FIXME: we shouldn't have todo this */
		/* For some reason, wait_for_services waits for the logger proccess
		 * to finish as well, but only on FreeBSD. We cannot allow this so
		 * we stop logging now. */
		rc_logger_close();
#endif

		rc_plugin_run(RC_HOOK_RUNLEVEL_STOP_IN, newlevel);
	} else {
		rc_plugin_run(RC_HOOK_RUNLEVEL_STOP_IN, runlevel);
	}
	hook_out = RC_HOOK_RUNLEVEL_STOP_OUT;

	/* Check if runlevel is valid if we're changing */
	if (newlevel && strcmp(runlevel, newlevel) != 0 && ! going_down) {
		if (! rc_runlevel_exists(newlevel))
			eerrorx ("%s: is not a valid runlevel", newlevel);
	}

	/* Load our deptree */
	if ((deptree = _rc_deptree_load(&regen)) == NULL)
		eerrorx("failed to load deptree");

	/* Clean the failed services state dir */
	clean_failed();

	if (mkdir(RC_STOPPING, 0755) != 0) {
		if (errno == EACCES)
			eerrorx("%s: superuser access required", applet);
		eerrorx("%s: failed to create stopping dir `%s': %s",
			applet, RC_STOPPING, strerror(errno));
	}

	/* Build a list of all services to stop and then work out the
	 * correct order for stopping them */
	stop_services = rc_services_in_state(RC_SERVICE_STARTED);
	tmplist = rc_services_in_state(RC_SERVICE_INACTIVE);
	if (tmplist) {
		if (stop_services) {
			TAILQ_CONCAT(stop_services, tmplist, entries);
			free(tmplist);
		} else
			stop_services = tmplist;
	}
	tmplist = rc_services_in_state(RC_SERVICE_STARTING);
	if (tmplist) {
		if (stop_services) {
			TAILQ_CONCAT(stop_services, tmplist, entries);
			free(tmplist);
		} else
			stop_services = tmplist;
	}
	if (stop_services)
		rc_stringlist_sort(&stop_services);

	types_nua = rc_stringlist_new();
	rc_stringlist_add(types_nua, "ineed");
	rc_stringlist_add(types_nua, "iuse");
	rc_stringlist_add(types_nua, "iafter");

	if (stop_services) {
		tmplist = rc_deptree_depends(deptree, types_nua, stop_services,
				runlevel, depoptions | RC_DEP_STOP);
		rc_stringlist_free(stop_services);
		stop_services = tmplist;
	}

	/* Load our list of coldplugged services */
	coldplugged_services = rc_services_in_state(RC_SERVICE_COLDPLUGGED);
	if (strcmp(newlevel ? newlevel : runlevel, RC_LEVEL_SINGLE) != 0 &&
	    strcmp(newlevel ? newlevel : runlevel, RC_LEVEL_SHUTDOWN) != 0 &&
	    strcmp(newlevel ? newlevel : runlevel, RC_LEVEL_REBOOT) != 0)
	{
		/* We need to include the boot runlevel services if we're not in it */
		start_services = rc_services_in_runlevel(bootlevel);
		if (strcmp (newlevel ? newlevel : runlevel, bootlevel) != 0) {
			tmplist = rc_services_in_runlevel(newlevel ? newlevel : runlevel);
			if (tmplist) {
				if (start_services) {
					TAILQ_CONCAT(start_services, tmplist, entries);
					free(tmplist);
				} else
					start_services = tmplist;
			}
		}

		if (coldplugged_services)
			TAILQ_FOREACH(service, coldplugged_services, entries)
				rc_stringlist_addu(start_services, service->value);
	}

	/* Save our softlevel now */
	if (going_down)
		rc_runlevel_set(newlevel);

	parallel = rc_conf_yesno("rc_parallel");

	/* Now stop the services that shouldn't be running */
	if (stop_services)
		do_stop_services(newlevel, parallel, going_down);

	/* Wait for our services to finish */
	wait_for_services();

	/* Notify the plugins we have finished */
	rc_plugin_run(RC_HOOK_RUNLEVEL_STOP_OUT, runlevel);
	hook_out = 0;

	rmdir(RC_STOPPING);

	/* Store the new runlevel */
	if (newlevel) {
		rc_runlevel_set(newlevel);
		free(runlevel);
		runlevel = xstrdup(newlevel);
		setenv("RC_RUNLEVEL", runlevel, 1);
	}

	/* Run the halt script if needed */
	if (strcmp(runlevel, RC_LEVEL_SHUTDOWN) == 0 ||
	    strcmp(runlevel, RC_LEVEL_REBOOT) == 0)
	{
		rc_logger_close();
		execl(HALTSH, HALTSH, runlevel, (char *) NULL);
		eerrorx("%s: unable to exec `%s': %s",
			 applet, HALTSH, strerror(errno));
	}

	/* Single user is done now */
	if (strcmp(runlevel, RC_LEVEL_SINGLE) == 0) {
		if (exists(INTERACTIVE))
			unlink(INTERACTIVE);
		sulogin(false);
	}

	mkdir(RC_STARTING, 0755);
	rc_plugin_run(RC_HOOK_RUNLEVEL_START_IN, runlevel);
	hook_out = RC_HOOK_RUNLEVEL_START_OUT;

	/* Re-add our coldplugged services if they stopped */
	if (coldplugged_services)
		TAILQ_FOREACH(service, coldplugged_services, entries)
			rc_service_mark(service->value, RC_SERVICE_COLDPLUGGED);

	/* Order the services to start */
	if (start_services) {
		rc_stringlist_sort(&start_services);
		deporder = rc_deptree_depends(deptree, types_nua, start_services,
				      runlevel, depoptions | RC_DEP_START);
		rc_stringlist_free(start_services);
		start_services = deporder;
	}

#ifdef __linux__
	/* mark any services skipped as started */
	if (PREVLEVEL && strcmp(PREVLEVEL, "N") == 0) {
		proc = p = proc_getent("noinitd");
		if (proc) {
			while ((token = strsep(&p, ",")))
				rc_service_mark(token, RC_SERVICE_STARTED);
			free(proc);
		}
	}
#endif

	if (start_services) {
		do_start_services(parallel);

		/* Wait for our services to finish */
		wait_for_services();
	}

	rc_plugin_run(RC_HOOK_RUNLEVEL_START_OUT, runlevel);
	hook_out = 0;

#ifdef __linux__
	/* mark any services skipped as stopped */
	if (PREVLEVEL && strcmp(PREVLEVEL, "N") == 0) {
		proc = p = proc_getent("noinitd");
		if (proc) {
			while ((token = strsep(&p, ",")))
				rc_service_mark(token, RC_SERVICE_STOPPED);
			free(proc);
		}
	}
#endif

	/* If we're in the boot runlevel and we regenerated our dependencies
	 * we need to delete them so that they are regenerated again in the
	 * default runlevel as they may depend on things that are now available */
	if (regen && strcmp(runlevel, bootlevel) == 0)
		unlink(RC_DEPTREE_CACHE);

	return EXIT_SUCCESS;
}

