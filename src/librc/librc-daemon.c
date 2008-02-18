/*
   librc-daemon
   Finds PID for given daemon criteria
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

#include "librc.h"

#if defined(__linux__)
static bool pid_is_cmd (pid_t pid, const char *cmd)
{
	char buffer[32];
	FILE *fp;
	int c;

	snprintf(buffer, sizeof (buffer), "/proc/%d/stat", pid);
	if ((fp = fopen (buffer, "r")) == NULL)
		return (false);

	while ((c = getc (fp)) != EOF && c != '(')
		;

	if (c != '(') {
		fclose(fp);
		return (false);
	}

	while ((c = getc (fp)) != EOF && c == *cmd)
		cmd++;

	fclose (fp);

	return ((c == ')' && *cmd == '\0') ? true : false);
}

static bool pid_is_exec (pid_t pid, const char *const *argv)
{
	char cmdline[32];
	char buffer[PATH_MAX];
	char *p;
	int fd = -1;
	int r;

	snprintf (cmdline, sizeof (cmdline), "/proc/%u/exe", pid);
	memset (buffer, 0, sizeof (buffer));
#if 0
	if (readlink (cmdline, buffer, sizeof (buffer)) != -1) {
		if (strcmp (exec, buffer) == 0)
			return (true);

		/* We should cater for deleted binaries too */
		if (strlen (buffer) > 10) {
			p = buffer + (strlen (buffer) - 10);
			if (strcmp (p, " (deleted)") == 0) {
				*p = 0;
				if (strcmp (buffer, exec) == 0)
					return (true);
			}
		}
	}
#endif

	snprintf (cmdline, sizeof (cmdline), "/proc/%u/cmdline", pid);
	if ((fd = open (cmdline, O_RDONLY)) < 0)
		return (false);

	r = read (fd, buffer, sizeof (buffer));
	close (fd);

	if (r == -1)
		return 0;

	buffer[r] = 0;
	p = buffer;
	while (*argv) {
		if (strcmp (*argv, p) != 0)
			return (false);
		argv++;
		p += strlen (p) + 1;
		if ((unsigned) (p - buffer) > sizeof (buffer))
			return (false);
	}
	return (true);
}

pid_t *rc_find_pids (const char *const *argv, const char *cmd,
		     uid_t uid, pid_t pid)
{
	DIR *procdir;
	struct dirent *entry;
	int npids = 0;
	pid_t p;
	pid_t *pids = NULL;
	pid_t *tmp = NULL;
	char buffer[PATH_MAX];
	struct stat sb;
	pid_t runscript_pid = 0;
	char *pp;

	if ((procdir = opendir ("/proc")) == NULL)
		return (NULL);

	/*
	   We never match RC_RUNSCRIPT_PID if present so we avoid the below
	   scenario

	   /etc/init.d/ntpd stop does
	   start-stop-daemon --stop --name ntpd
	   catching /etc/init.d/ntpd stop

	   nasty
	   */

	if ((pp = getenv ("RC_RUNSCRIPT_PID"))) {
		if (sscanf (pp, "%d", &runscript_pid) != 1)
			runscript_pid = 0;
	}

	while ((entry = readdir (procdir)) != NULL) {
		if (sscanf (entry->d_name, "%d", &p) != 1)
			continue;

		if (runscript_pid != 0 && runscript_pid == p)
			continue;

		if (pid != 0 && pid != p)
			continue;

		if (uid) {
			snprintf (buffer, sizeof (buffer), "/proc/%d", p);
			if (stat (buffer, &sb) != 0 || sb.st_uid != uid)
				continue;
		}

		if (cmd && ! pid_is_cmd (p, cmd))
			continue;

		if (argv && ! cmd && ! pid_is_exec (p, (const char *const *)argv))
			continue;

		tmp = realloc (pids, sizeof (pid_t) * (npids + 2));
		if (! tmp) {
			free (pids);
			closedir (procdir);
			errno = ENOMEM;
			return (NULL);
		}
		pids = tmp;

		pids[npids] = p;
		pids[npids + 1] = 0;
		npids++;
	}
	closedir (procdir);

	return (pids);
}
librc_hidden_def(rc_find_pids)

#elif BSD

# if defined(__DragonFly__) || defined(__FreeBSD__)
#  ifndef KERN_PROC_PROC
#    define KERN_PROC_PROC KERN_PROC_ALL
#  endif
#  define _KINFO_PROC kinfo_proc
#  define _KVM_GETARGV kvm_getargv
#  define _GET_KINFO_UID(kp) (kp.ki_ruid)
#  define _GET_KINFO_COMM(kp) (kp.ki_comm)
#  define _GET_KINFO_PID(kp) (kp.ki_pid)
#  define _KVM_PATH _PATH_DEVNULL
# else
#  define _KVM_GETPROC2
#  define _KINFO_PROC kinfo_proc2
#  define _KVM_GETARGV kvm_getargv2
#  define _GET_KINFO_UID(kp) (kp.p_ruid)
#  define _GET_KINFO_COMM(kp) (kp.p_comm)
#  define _GET_KINFO_PID(kp) (kp.p_pid)
#  define _KVM_PATH NULL
# endif

pid_t *rc_find_pids (const char *const *argv, const char *cmd,
		     uid_t uid, pid_t pid)
{
	static kvm_t *kd = NULL;
	char errbuf[_POSIX2_LINE_MAX];
	struct _KINFO_PROC *kp;
	int i;
	int processes = 0;
	int pargc = 0;
	char **pargv;
	pid_t *pids = NULL;
	pid_t *tmp;
	char *arg;
	int npids = 0;
	int match;

	if ((kd = kvm_openfiles (_KVM_PATH, _KVM_PATH,
				 NULL, O_RDONLY, errbuf)) == NULL)
	{
		fprintf (stderr, "kvm_open: %s\n", errbuf);
		return (NULL);
	}

#ifdef _KVM_GETPROC2
	kp = kvm_getproc2 (kd, KERN_PROC_ALL, 0, sizeof(*kp), &processes);
#else
	kp = kvm_getprocs (kd, KERN_PROC_PROC, 0, &processes);
#endif
	if ((kp == NULL && processes > 0) || (kp != NULL && processes < 0)) {
		fprintf (stderr, "kvm_getprocs: %s\n", kvm_geterr (kd));
		kvm_close (kd);
		return (NULL);
	}

	for (i = 0; i < processes; i++) {
		pid_t p = _GET_KINFO_PID (kp[i]);
		if (pid != 0 && pid != p)
			continue;

		if (uid != 0 && uid != _GET_KINFO_UID (kp[i]))
			continue;

		if (cmd) {
			if (! _GET_KINFO_COMM (kp[i]) ||
			    strcmp (cmd, _GET_KINFO_COMM (kp[i])) != 0)
				continue;
		}

		if (argv && ! cmd) {
			pargv = _KVM_GETARGV (kd, &kp[i], pargc);
			if (! pargv || ! *pargv)
				continue;

			arg = argv;
			match = 1;

			while (arg && *pargv)
				if (strcmp (arg++, *pargv++) != 0) {
					match = 0;
					break;
				}

			if (! match)
				continue;
		}

		tmp = realloc (pids, sizeof (pid_t) * (npids + 2));
		if (! tmp) {
			free (pids);
			kvm_close (kd);
			errno = ENOMEM;
			return (NULL);
		}
		pids = tmp;

		pids[npids] = p;
		pids[npids + 1] = 0;
		npids++;
	}
	kvm_close (kd);

	return (pids);
}
librc_hidden_def(rc_find_pids)

#else
#  error "Platform not supported!"
#endif

static bool _match_daemon (const char *path, const char *file,
			   const char *mexec, const char *mname,
			   const char *mpidfile)
{
	char *line;
	char *ffile = rc_strcatpaths (path, file, (char *) NULL);
	FILE *fp;
	int lc = 0;
	int m = 0;

	if ((fp = fopen (ffile, "r")) == NULL) {
		free (ffile);
		return (false);
	}

	if (! mname)
		m += 10;
	if (! mpidfile)
		m += 100;

	while ((line = rc_getline (fp))) {
		if (strcmp (line, mexec) == 0)
			m += 1;
		else if (mname && strcmp (line, mname) == 0)
			m += 10;
		else if (mpidfile && strcmp (line, mpidfile) == 0)
			m += 100;
		free (line);

		if (m == 111)
			break;

		lc++;
		if (lc > 5)
			break;
	}
	fclose (fp);
	free (ffile);

	return (m == 111 ? true : false);
}

bool rc_service_daemon_set (const char *service, const char *const *argv,
			    const char *name, const char *pidfile,
			    bool started)
{
	char *dirpath;
	char *file = NULL;
	size_t l;
	char *mexec;
	char *mname;
	char *mpidfile;
	int nfiles = 0;
	char *oldfile = NULL;
	bool retval = false;
	DIR *dp;
	struct dirent *d;

	if (! (argv && *argv) && ! name && ! pidfile) {
		errno = EINVAL;
		return (false);
	}

	dirpath = rc_strcatpaths (RC_SVCDIR, "daemons",
				  basename_c (service), (char *) NULL);

	if (argv && *argv) {
		l = strlen (*argv) + 6;
		mexec = xmalloc (sizeof (char) * l);
		snprintf (mexec, l, "exec=%s", *argv);
	} else
		mexec = xstrdup ("exec=");

	if (name) {
		l = strlen (name) + 6;
		mname = xmalloc (sizeof (char) * l);
		snprintf (mname, l, "name=%s", name);
	} else
		mname = xstrdup ("name=");

	if (pidfile) {
		l = strlen (pidfile) + 9;
		mpidfile = xmalloc (sizeof (char) * l);
		snprintf (mpidfile, l, "pidfile=%s", pidfile);
	} else
		mpidfile = xstrdup ("pidfile=");

	/* Regardless, erase any existing daemon info */
	if ((dp = opendir (dirpath))) {
		while ((d = readdir (dp))) {
			if (d->d_name[0] == '.')
				continue;
			file = rc_strcatpaths (dirpath, d->d_name, (char *) NULL);
			nfiles++;

			if (! oldfile) {
				if (_match_daemon (dirpath, d->d_name,
						   mexec, mname, mpidfile))
				{
					unlink (file);
					oldfile = file;
					nfiles--;
				}
			} else {
				rename (file, oldfile);
				free (oldfile);
				oldfile = file;
			}
		}
		free (file);
		closedir (dp);
	}

	/* Now store our daemon info */
	if (started) {
		char buffer[10];
		FILE *fp;

		if (mkdir (dirpath, 0755) == 0 || errno == EEXIST) {
			snprintf (buffer, sizeof (buffer), "%03d", nfiles + 1);
			file = rc_strcatpaths (dirpath, buffer, (char *) NULL);
			if ((fp = fopen (file, "w"))) {
				fprintf (fp, "%s\n%s\n%s\n", mexec, mname, mpidfile);
				fclose (fp);
				retval = true;
			}
			free (file);
		}
	} else
		retval = true;

	free (mexec);
	free (mname);
	free (mpidfile);
	free (dirpath);

	return (retval);
}
librc_hidden_def(rc_service_daemon_set)

bool rc_service_started_daemon (const char *service, const char *exec,
				int indx)
{
	char *dirpath;
	char *file;
	size_t l;
	char *mexec;
	bool retval = false;
	DIR *dp;
	struct dirent *d;

	if (! service || ! exec)
		return (false);

	dirpath = rc_strcatpaths (RC_SVCDIR, "daemons", basename_c (service),
				  (char *) NULL);

	l = strlen (exec) + 6;
	mexec = xmalloc (sizeof (char) * l);
	snprintf (mexec, l, "exec=%s", exec);

	if (indx > 0) {
		l = sizeof (char) * 10;
		file = xmalloc (l);
		snprintf (file, l, "%03d", indx);
		retval = _match_daemon (dirpath, file, mexec, NULL, NULL);
		free (file);
	} else {
		if ((dp = opendir (dirpath))) {
			while ((d = readdir (dp))) {
				if (d->d_name[0] == '.')
					continue;
				retval = _match_daemon (dirpath, d->d_name, mexec, NULL, NULL);
				if (retval)
					break;
			}
			closedir (dp);
		}
	}

	free (dirpath);
	free (mexec);
	return (retval);
}
librc_hidden_def(rc_service_started_daemon)

bool rc_service_daemons_crashed (const char *service)
{
	char *dirpath;
	DIR *dp;
	struct dirent *d;
	char *path;
	FILE *fp;
	char *line;
	char **argv = NULL;
	char *exec = NULL;
	char *name = NULL;
	char *pidfile = NULL;
	pid_t pid = 0;
	pid_t *pids = NULL;
	char *p;
	char *token;
	bool retval = false;

	if (! service)
		return (false);

	dirpath = rc_strcatpaths (RC_SVCDIR, "daemons", basename_c (service),
				  (char *) NULL);

	if (! (dp = opendir (dirpath))) {
		free (dirpath);
		return (false);
	}

	while ((d = readdir (dp))) {
		if (d->d_name[0] == '.')
			continue;

		path = rc_strcatpaths (dirpath, d->d_name, (char *) NULL);
		fp = fopen (path, "r");
		free (path);
		if (! fp)
			break;

		while ((line = rc_getline (fp))) {
			p = line;
			if ((token = strsep (&p, "=")) == NULL || ! p) {
				free (line);
				continue;
			}

			if (strlen (p) == 0) {
				free (line);
				continue;
			}

			if (strcmp (token, "argv") == 0) {
				rc_strlist_add (&argv, p);
			} else if (strcmp (token, "exec") == 0) {
				if (exec)
					free (exec);
				exec = xstrdup (p);
			} else if (strcmp (token, "name") == 0) {
				if (name)
					free (name);
				name = xstrdup (p);
			} else if (strcmp (token, "pidfile") == 0) {
				if (pidfile)
					free (pidfile);
				pidfile = xstrdup (p);
			}
			free (line);
		}
		fclose (fp);

		pid = 0;
		if (pidfile) {
			if (! exists (pidfile)) {
				retval = true;
				break;
			}

			if ((fp = fopen (pidfile, "r")) == NULL) {
				retval = true;
				break;
			}

			if (fscanf (fp, "%d", &pid) != 1) {
				fclose (fp);
				retval = true;
				break;
			}

			fclose (fp);
			free (pidfile);
			pidfile = NULL;

			/* We have the pid, so no need to match on name */
			rc_strlist_free (argv);
			argv = NULL;
			free (exec);
			exec = NULL;
			free (name);
			name = NULL;
		}

		if (exec && ! argv) {
			rc_strlist_add (&argv, exec);
			free (exec);
			exec = NULL;
		}

		if ((pids = rc_find_pids ((const char *const *)argv, name, 0, pid)) == NULL) {
			retval = true;
			break;
		}
		free (pids);

		rc_strlist_free (argv);
		argv = NULL;
		free (exec);
		exec = NULL;
		free (name);
		name = NULL;
	}

	rc_strlist_free (argv);
	free (exec);
	free (name);
	free (dirpath);
	closedir (dp);

	return (retval);
}
librc_hidden_def(rc_service_daemons_crashed)
