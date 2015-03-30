/*
  librc-daemon
  Finds PID for given daemon criteria
*/

/*
 * Copyright (c) 2007-2009 Roy Marples <roy@marples.name>
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

#include "queue.h"
#include "librc.h"

#if defined(__linux__) || (defined (__FreeBSD_kernel__) && defined(__GLIBC__))
static bool
pid_is_exec(pid_t pid, const char *exec)
{
	char buffer[32];
	FILE *fp;
	int c;
	bool retval = false;

	exec = basename_c(exec);
	snprintf(buffer, sizeof(buffer), "/proc/%d/stat", pid);
	if ((fp = fopen(buffer, "r"))) {
		while ((c = getc(fp)) != EOF && c != '(')
			;
		if (c == '(') {
			while ((c = getc(fp)) != EOF && c == *exec)
				exec++;
			if (c == ')' && *exec == '\0')
				retval = true;
		}
		fclose(fp);
	}
	return retval;
}

static bool
pid_is_argv(pid_t pid, const char *const *argv)
{
	char cmdline[32];
	int fd;
	char buffer[PATH_MAX];
	char *p;
	ssize_t bytes;

	snprintf(cmdline, sizeof(cmdline), "/proc/%u/cmdline", pid);
	if ((fd = open(cmdline, O_RDONLY)) < 0)
		return false;
	bytes = read(fd, buffer, sizeof(buffer));
	close(fd);
	if (bytes == -1)
		return false;

	buffer[bytes] = '\0';
	p = buffer;
	while (*argv) {
		if (strcmp(*argv, p) != 0)
			return false;
		argv++;
		p += strlen(p) + 1;
		if ((unsigned)(p - buffer) > sizeof(buffer))
			return false;
	}
	return true;
}

RC_PIDLIST *
rc_find_pids(const char *exec, const char *const *argv, uid_t uid, pid_t pid)
{
	DIR *procdir;
	struct dirent *entry;
	FILE *fp;
	bool container_pid = false;
	bool openvz_host = false;
	char *line = NULL;
	size_t len = 0;
	pid_t p;
	char buffer[PATH_MAX];
	struct stat sb;
	pid_t runscript_pid = 0;
	char *pp;
	RC_PIDLIST *pids = NULL;
	RC_PID *pi;

	if ((procdir = opendir("/proc")) == NULL)
		return NULL;

	/*
	  We never match RC_RUNSCRIPT_PID if present so we avoid the below
	  scenario

	  /etc/init.d/ntpd stop does
	  start-stop-daemon --stop --name ntpd
	  catching /etc/init.d/ntpd stop

	  nasty
	*/

	if ((pp = getenv("RC_RUNSCRIPT_PID"))) {
		if (sscanf(pp, "%d", &runscript_pid) != 1)
			runscript_pid = 0;
	}

	/*
	If /proc/self/status contains EnvID: 0, then we are an OpenVZ host,
	and we will need to filter out processes that are inside containers
	from our list of pids.
	*/

	if (exists("/proc/self/status")) {
		fp = fopen("/proc/self/status", "r");
		if (fp) {
			while (! feof(fp)) {
				rc_getline(&line, &len, fp);
				if (strncmp(line, "envID:\t0", 8) == 0) {
					openvz_host = true;
					break;
				}
			}
			fclose(fp);
		}
	}

	while ((entry = readdir(procdir)) != NULL) {
		if (sscanf(entry->d_name, "%d", &p) != 1)
			continue;
		if (runscript_pid != 0 && runscript_pid == p)
			continue;
		if (pid != 0 && pid != p)
			continue;
		if (uid) {
			snprintf(buffer, sizeof(buffer), "/proc/%d", p);
			if (stat(buffer, &sb) != 0 || sb.st_uid != uid)
				continue;
		}
		if (exec && !pid_is_exec(p, exec))
			continue;
		if (argv &&
		    !pid_is_argv(p, (const char *const *)argv))
			continue;
		/* If this is an OpenVZ host, filter out container processes */
		if (openvz_host) {
			snprintf(buffer, sizeof(buffer), "/proc/%d/status", p);
			if (exists(buffer)) {
				fp = fopen(buffer, "r");
				if (! fp)
					continue;
				while (! feof(fp)) {
					rc_getline(&line, &len, fp);
					if (strncmp(line, "envID:", 6) == 0) {
						container_pid = ! (strncmp(line, "envID:\t0", 8) == 0);
						break;
					}
				}
				fclose(fp);
			}
		}
		if (container_pid)
			continue;
		if (!pids) {
			pids = xmalloc(sizeof(*pids));
			LIST_INIT(pids);
		}
		pi = xmalloc(sizeof(*pi));
		pi->pid = p;
		LIST_INSERT_HEAD(pids, pi, entries);
	}
	if (line != NULL)
		free(line);
	closedir(procdir);
	return pids;
}
librc_hidden_def(rc_find_pids)

#elif BSD

# if defined(__NetBSD__) || defined(__OpenBSD__)
#  define _KVM_GETPROC2
#  define _KINFO_PROC kinfo_proc2
#  define _KVM_GETARGV kvm_getargv2
#  define _GET_KINFO_UID(kp) (kp.p_ruid)
#  define _GET_KINFO_COMM(kp) (kp.p_comm)
#  define _GET_KINFO_PID(kp) (kp.p_pid)
#  define _KVM_PATH NULL
#  define _KVM_FLAGS KVM_NO_FILES
# else
#  ifndef KERN_PROC_PROC
#    define KERN_PROC_PROC KERN_PROC_ALL
#  endif
#  define _KINFO_PROC kinfo_proc
#  define _KVM_GETARGV kvm_getargv
#  if defined(__DragonFly__)
#    define _GET_KINFO_UID(kp) (kp.kp_ruid)
#    define _GET_KINFO_COMM(kp) (kp.kp_comm)
#    define _GET_KINFO_PID(kp) (kp.kp_pid)
#  else
#    define _GET_KINFO_UID(kp) (kp.ki_ruid)
#    define _GET_KINFO_COMM(kp) (kp.ki_comm)
#    define _GET_KINFO_PID(kp) (kp.ki_pid)
#  endif
#  define _KVM_PATH _PATH_DEVNULL
#  define _KVM_FLAGS O_RDONLY
# endif

RC_PIDLIST *
rc_find_pids(const char *exec, const char *const *argv, uid_t uid, pid_t pid)
{
	static kvm_t *kd = NULL;
	char errbuf[_POSIX2_LINE_MAX];
	struct _KINFO_PROC *kp;
	int i;
	int processes = 0;
	int pargc = 0;
	char **pargv;
	RC_PIDLIST *pids = NULL;
	RC_PID *pi;
	pid_t p;
	const char *const *arg;
	int match;

	if ((kd = kvm_openfiles(_KVM_PATH, _KVM_PATH,
		    NULL, _KVM_FLAGS, errbuf)) == NULL)
	{
		fprintf(stderr, "kvm_open: %s\n", errbuf);
		return NULL;
	}

#ifdef _KVM_GETPROC2
	kp = kvm_getproc2(kd, KERN_PROC_ALL, 0, sizeof(*kp), &processes);
#else
	kp = kvm_getprocs(kd, KERN_PROC_PROC, 0, &processes);
#endif
	if ((kp == NULL && processes > 0) || (kp != NULL && processes < 0)) {
		fprintf(stderr, "kvm_getprocs: %s\n", kvm_geterr(kd));
		kvm_close(kd);
		return NULL;
	}

	if (exec)
		exec = basename_c(exec);
	for (i = 0; i < processes; i++) {
		p = _GET_KINFO_PID(kp[i]);
		if (pid != 0 && pid != p)
			continue;
		if (uid != 0 && uid != _GET_KINFO_UID(kp[i]))
			continue;
		if (exec) {
			if (!_GET_KINFO_COMM(kp[i]) ||
			    strcmp(exec, _GET_KINFO_COMM(kp[i])) != 0)
				continue;
		}
		if (argv && *argv) {
			pargv = _KVM_GETARGV(kd, &kp[i], pargc);
			if (!pargv || !*pargv)
				continue;
			arg = argv;
			match = 1;
			while (*arg && *pargv)
				if (strcmp(*arg++, *pargv++) != 0) {
					match = 0;
					break;
				}
			if (!match)
				continue;
		}
		if (!pids) {
			pids = xmalloc(sizeof(*pids));
			LIST_INIT(pids);
		}
		pi = xmalloc(sizeof(*pi));
		pi->pid = p;
		LIST_INSERT_HEAD(pids, pi, entries);
	}
	kvm_close(kd);

	return pids;
}
librc_hidden_def(rc_find_pids)

#elif defined(__QNX__)
#include <sys/procfs.h>
#include <stdint.h>

typedef struct {
	procfs_debuginfo		info;
	char					buff[_POSIX_PATH_MAX];
} procfs_name;


/* This logic is derived from QNX's 'pidin arg' call
   It opens up the process memory and reads the initial stack
   Unforunately, the original command line is not kept, and processes
   can write to this space (basename() is an easy example), so it could 
   potentially be fooled.
*/ 
static bool is_argv(int fd, const char* path, const procfs_info* info, const char *const * argv) {
	char p[PATH_MAX];
	char *args[32];
	int	 argc, valid, i;
	off_t pos;    
	int	num;
	
    
    /* Read the argc, argv from the process' address space */
    if (!info->initial_stack) {
        /* No stack ptr available; just try the path */
        if (strcmp(*argv,path) != 0) {
            return false;
        };
        ++argv;
        return (*argv == 0);        
    };
    
	if (lseek64(fd, info->initial_stack, SEEK_SET) == -1) {
	    /* Couldn't seek to stack */
		return false;
	}
	if ((valid = read(fd, p, sizeof(argc))) < (int)sizeof(argc)) {
	    /* Couldn't read stack */
	    return false;
	}
	argc = *(int *) p;
	pos = info->initial_stack + sizeof(argc);
	
	while (argc) {

		num = min(argc, 32);
		if (read(fd, args, num * sizeof args[0]) < num * (int)sizeof args[0]) {
		    /* Couldn't read argv ptrs from stack */
			return false;
		}
		argc -= num;

		for (i = 0; i < num; i++) {
			/* don't continue past a null argv[i] pointer */
			if ( args[i] == 0 ) {
				return false;
			}

			if (lseek64(fd, (off64_t)(uintptr_t)args[i], SEEK_SET) == -1) {
			    /* Couldn't seek to arg */
			    return false;
			}
			
			if ((valid = read(fd, p, sizeof p - 1)) == -1) {
				/* Couldn't read arg */
				return false;
			}
			p[valid] = 0;
			if (strcmp(*argv,p) != 0) { 
			    return false;
			};
			++argv;
			if (!*argv) return true;
		}
		pos += num * sizeof args[0];
		if (lseek64(fd, pos, SEEK_SET) == -1) {
			/* couldn't seek to argv stack */
			return false;
		}
	}    
    return false;  /* ran out of argc before argv */
};

RC_PIDLIST *
rc_find_pids(const char *exec, const char *const *argv, uid_t uid, pid_t pid)
{
	DIR *procdir;
	struct dirent *entry;
	pid_t p;
	char buffer[PATH_MAX];
	pid_t runscript_pid = 0;
	char *pp;
	RC_PIDLIST *pids = NULL;
	RC_PID *pi;
	int fd;
    procfs_name name;
    procfs_info	info;
    char exec_basename[PATH_MAX];
    char* process_basename;


	if ((procdir = opendir("/proc")) == NULL)
		return NULL;

    /* TODO: refactor this with the linux code above */
	/*
	  We never match RC_RUNSCRIPT_PID if present so we avoid the below
	  scenario

	  /etc/init.d/ntpd stop does
	  start-stop-daemon --stop --name ntpd
	  catching /etc/init.d/ntpd stop

	  nasty
	*/

	if ((pp = getenv("RC_RUNSCRIPT_PID"))) {
		if (sscanf(pp, "%d", &runscript_pid) != 1)
			runscript_pid = 0;
	}
	
	// We check only the basename of exec
	if (exec) {
	    strncpy(exec_basename,exec,sizeof(exec_basename));
	    exec = basename(exec_basename);
	};	

	while ((entry = readdir(procdir)) != NULL) {
		if (sscanf(entry->d_name, "%d", &p) != 1)
			continue;
		if (runscript_pid != 0 && runscript_pid == p)
			continue;
		if (pid != 0 && pid != p)
			continue;

        snprintf(buffer, sizeof(buffer), "/proc/%d/as", p);
        fd=open64(buffer,O_RDONLY);
        if (fd == -1) {
            /* Alas */
            continue;
        };
        
        if (devctl(fd, DCMD_PROC_INFO, &info, sizeof info, 0) != EOK) {
            /* Process has probably terminated before we got to look at it */
            close(fd);
            continue;
        };
        			
		if (uid && (info.uid != uid)) {			
            close(fd);
            continue;
		}
		
		if (devctl(fd, DCMD_PROC_MAPDEBUG_BASE, &name, sizeof name, 0) != EOK) {
		    close(fd);
		    continue;
		};
		
		// Save the full path
		buffer[0] = '/';
		strncpy(buffer+1,name.info.path,sizeof(buffer)-1);
		process_basename = basename(name.info.path);

		if (exec && (strcmp(exec,process_basename)) != 0) {
		    close(fd);
			continue;
		};
				
		if (argv && *argv && !is_argv(fd, buffer, &info, argv)) {
		    close(fd);
			continue;
        };
        close(fd);
			
		if (!pids) {
			pids = xmalloc(sizeof(*pids));
			LIST_INIT(pids);
		}
		pi = xmalloc(sizeof(*pi));
		pi->pid = p;
		LIST_INSERT_HEAD(pids, pi, entries);
	}

	closedir(procdir);
	return pids;
}
librc_hidden_def(rc_find_pids)


#else
#  error "Platform not supported!"
#endif

static bool
_match_daemon(const char *path, const char *file, RC_STRINGLIST *match)
{
	char *line = NULL;
	size_t len = 0;
	char ffile[PATH_MAX];
	FILE *fp;
	RC_STRING *m;

	snprintf(ffile, sizeof(ffile), "%s/%s", path, file);
	fp = fopen(ffile, "r");

	if (!fp)
		return false;

	while ((rc_getline(&line, &len, fp))) {
		TAILQ_FOREACH(m, match, entries)
		    if (strcmp(line, m->value) == 0) {
			    TAILQ_REMOVE(match, m, entries);
			    break;
		    }
		if (!TAILQ_FIRST(match))
			break;
	}
	fclose(fp);
	free(line);
	if (TAILQ_FIRST(match))
		return false;
	return true;
}

static RC_STRINGLIST *
_match_list(const char *exec, const char *const *argv, const char *pidfile)
{
	RC_STRINGLIST *match = rc_stringlist_new();
	int i = 0;
	size_t l;
	char *m;

	if (exec) {
		l = strlen(exec) + 6;
		m = xmalloc(sizeof(char) * l);
		snprintf(m, l, "exec=%s", exec);
		rc_stringlist_add(match, m);
		free(m);
	}

	while (argv && argv[i]) {
		l = strlen(*argv) + strlen("argv_=") + 16;
		m = xmalloc(sizeof(char) * l);
		snprintf(m, l, "argv_0=%s", argv[i++]);
		rc_stringlist_add(match, m);
		free(m);
	}

	if (pidfile) {
		l = strlen(pidfile) + 9;
		m = xmalloc(sizeof(char) * l);
		snprintf(m, l, "pidfile=%s", pidfile);
		rc_stringlist_add(match, m);
		free(m);
	}

	return match;
}

bool
rc_service_daemon_set(const char *service, const char *exec,
    const char *const *argv,
    const char *pidfile, bool started)
{
	char dirpath[PATH_MAX];
	char file[PATH_MAX];
	int nfiles = 0;
	char oldfile[PATH_MAX] = { '\0' };
	bool retval = false;
	DIR *dp;
	struct dirent *d;
	RC_STRINGLIST *match;
	int i = 0;
	FILE *fp;

	if (!exec && !pidfile) {
		errno = EINVAL;
		return false;
	}

	snprintf(dirpath, sizeof(dirpath), RC_SVCDIR "/daemons/%s",
	    basename_c(service));

	/* Regardless, erase any existing daemon info */
	if ((dp = opendir(dirpath))) {
		match = _match_list(exec, argv, pidfile);
		while ((d = readdir(dp))) {
			if (d->d_name[0] == '.')
				continue;

			snprintf(file, sizeof(file), "%s/%s",
			    dirpath, d->d_name);
			nfiles++;

			if (!*oldfile) {
				if (_match_daemon(dirpath, d->d_name, match)) {
					unlink(file);
					strlcpy(oldfile, file, sizeof(oldfile));
					nfiles--;
				}
			} else {
				rename(file, oldfile);
				strlcpy(oldfile, file, sizeof(oldfile));
			}
		}
		closedir(dp);
		rc_stringlist_free(match);
	}

	/* Now store our daemon info */
	if (started) {
		if (mkdir(dirpath, 0755) == 0 || errno == EEXIST) {
			snprintf(file, sizeof(file), "%s/%03d",
			    dirpath, nfiles + 1);
			if ((fp = fopen(file, "w"))) {
				fprintf(fp, "exec=");
				if (exec)
					fprintf(fp, "%s", exec);
				while (argv && argv[i]) {
					fprintf(fp, "\nargv_%d=%s", i, argv[i]);
					i++;
				}
				fprintf(fp, "\npidfile=");
				if (pidfile)
					fprintf(fp, "%s", pidfile);
				fprintf(fp, "\n");
				fclose(fp);
				retval = true;
			}
		}
	} else
		retval = true;

	return retval;
}
librc_hidden_def(rc_service_daemon_set)

bool
rc_service_started_daemon(const char *service,
    const char *exec, const char *const *argv, int indx)
{
	char dirpath[PATH_MAX];
	char file[16];
	RC_STRINGLIST *match;
	bool retval = false;
	DIR *dp;
	struct dirent *d;

	if (!service || !exec)
		return false;

	snprintf(dirpath, sizeof(dirpath), RC_SVCDIR "/daemons/%s",
	    basename_c(service));
	match = _match_list(exec, argv, NULL);

	if (indx > 0) {
		snprintf(file, sizeof(file), "%03d", indx);
		retval = _match_daemon(dirpath, file, match);
	} else {
		if ((dp = opendir(dirpath))) {
			while ((d = readdir(dp))) {
				if (d->d_name[0] == '.')
					continue;
				retval = _match_daemon(dirpath, d->d_name, match);
				if (retval)
					break;
			}
			closedir(dp);
		}
	}

	rc_stringlist_free(match);
	return retval;
}
librc_hidden_def(rc_service_started_daemon)

bool
rc_service_daemons_crashed(const char *service)
{
	char dirpath[PATH_MAX];
	DIR *dp;
	struct dirent *d;
	char *path = dirpath;
	FILE *fp;
	char *line = NULL;
	size_t len = 0;
	char **argv = NULL;
	char *exec = NULL;
	char *name = NULL;
	char *pidfile = NULL;
	pid_t pid = 0;
	RC_PIDLIST *pids;
	RC_PID *p1;
	RC_PID *p2;
	char *p;
	char *token;
	bool retval = false;
	RC_STRINGLIST *list = NULL;
	RC_STRING *s;
	size_t i;
	char *ch_root;
	char *spidfile;

	path += snprintf(dirpath, sizeof(dirpath), RC_SVCDIR "/daemons/%s",
	    basename_c(service));

	if (!(dp = opendir(dirpath)))
		return false;

	while ((d = readdir(dp))) {
		if (d->d_name[0] == '.')
			continue;

		snprintf(path, sizeof(dirpath) - (path - dirpath), "/%s",
		    d->d_name);
		fp = fopen(dirpath, "r");
		if (!fp)
			break;

		while ((rc_getline(&line, &len, fp))) {
			p = line;
			if ((token = strsep(&p, "=")) == NULL || !p)
				continue;

			if (!*p)
				continue;

			if (strcmp(token, "exec") == 0) {
				if (exec)
					free(exec);
				exec = xstrdup(p);
			} else if (strncmp(token, "argv_", 5) == 0) {
				if (!list)
					list = rc_stringlist_new();
				rc_stringlist_add(list, p);
			} else if (strcmp(token, "name") == 0) {
				if (name)
					free(name);
				name = xstrdup(p);
			} else if (strcmp(token, "pidfile") == 0) {
				pidfile = xstrdup(p);
				break;
			}
		}
		fclose(fp);

		ch_root = rc_service_value_get(basename_c(service), "chroot");
		spidfile = pidfile;
		if (ch_root && pidfile) {
			spidfile = xmalloc(strlen(ch_root) + strlen(pidfile) + 1);
			strcpy(spidfile, ch_root);
			strcat(spidfile, pidfile);
			free(pidfile);
			pidfile = spidfile;
		}

		pid = 0;
		if (pidfile) {
			retval = true;
			if ((fp = fopen(pidfile, "r"))) {
				if (fscanf(fp, "%d", &pid) == 1)
					retval = false;
				fclose(fp);
			}
			free(pidfile);
			pidfile = NULL;

			/* We have the pid, so no need to match
			   on exec or name */
			free(exec);
			exec = NULL;
			free(name);
			name = NULL;
		} else {
			if (exec) {
				if (!list)
					list = rc_stringlist_new();
				if (!TAILQ_FIRST(list))
					rc_stringlist_add(list, exec);

				free(exec);
				exec = NULL;
			}

			if (list) {
				/* We need to flatten our linked list
				   into an array */
				i = 0;
				TAILQ_FOREACH(s, list, entries)
				    i++;
				argv = xmalloc(sizeof(char *) * (i + 1));
				i = 0;
				TAILQ_FOREACH(s, list, entries)
				    argv[i++] = s->value;
				argv[i] = '\0';
			}
		}

		if (!retval) {
			if (pid != 0) {
				if (kill(pid, 0) == -1 && errno == ESRCH)
					retval = true;
			} else if ((pids = rc_find_pids(exec,
				    (const char *const *)argv,
				    0, pid)))
			{
				p1 = LIST_FIRST(pids);
				while (p1) {
					p2 = LIST_NEXT(p1, entries);
					free(p1);
					p1 = p2;
				}
				free(pids);
			} else
				retval = true;
		}
		rc_stringlist_free(list);
		list = NULL;
		free(argv);
		argv = NULL;
		free(exec);
		exec = NULL;
		free(name);
		name = NULL;
		if (retval)
			break;
	}
	closedir(dp);
	free(line);

	return retval;
}
librc_hidden_def(rc_service_daemons_crashed)
