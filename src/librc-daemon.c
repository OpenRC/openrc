/*
   librc-daemon
   Finds PID for given daemon criteria
   Copyright 2007 Gentoo Foundation
   Released under the GPLv2
   */

#include <sys/types.h>
#include <sys/stat.h>

#if defined(__FreeBSD__) || defined(__NetBSD__) || defined (__OpenBSD__) 
#include <sys/param.h>
#include <sys/user.h>
#include <sys/sysctl.h>
#include <kvm.h>
#include <limits.h>
#endif

#ifndef __linux__
#include <libgen.h>
#endif

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "einfo.h"
#include "rc.h"
#include "rc-misc.h"
#include "strlist.h"

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

  if (c != '(')
    {
      fclose(fp);
      return (false);
    }

  while ((c = getc (fp)) != EOF && c == *cmd)
    cmd++;

  fclose (fp);

  return ((c == ')' && *cmd == '\0') ? true : false);
}

static bool pid_is_exec (pid_t pid, const char *exec)
{
  char cmdline[32];
  char buffer[PATH_MAX];
  char *p;
  int fd = -1;
  int r;

  snprintf (cmdline, sizeof (cmdline), "/proc/%u/exe", pid);
  memset (buffer, 0, sizeof (buffer));
  if (readlink (cmdline, buffer, sizeof (buffer)) != -1)
    {
      if (strcmp (exec, buffer) == 0)
	return (true);

      /* We should cater for deleted binaries too */
      if (strlen (buffer) > 10)
	{
	  p = buffer + (strlen (buffer) - 10);
	  if (strcmp (p, " (deleted)") == 0)
	    {
	      *p = 0;
	      if (strcmp (buffer, exec) == 0)
		return (true);
	    }
	}
    }

  snprintf (cmdline, sizeof (cmdline), "/proc/%u/cmdline", pid);
  if ((fd = open (cmdline, O_RDONLY)) < 0)
    return (false);

  r = read(fd, buffer, sizeof (buffer));
  close (fd);

  if (r == -1)
    return 0;

  buffer[r] = 0;
  return (strcmp (exec, buffer) == 0 ? true : false);
}

pid_t *rc_find_pids (const char *exec, const char *cmd,
		     uid_t uid, pid_t pid)
{
  DIR *procdir;
  struct dirent *entry;
  int npids = 0;
  int foundany = false;
  pid_t p;
  pid_t *pids = NULL;
  char buffer[PATH_MAX];
  struct stat sb;
  pid_t runscript_pid = 0;
  char *pp;

  if ((procdir = opendir ("/proc")) == NULL)
    eerrorx ("opendir `/proc': %s", strerror (errno));

  /*
     We never match RC_RUNSCRIPT_PID if present so we avoid the below
     scenario

     /etc/init.d/ntpd stop does
     start-stop-daemon --stop --name ntpd
     catching /etc/init.d/ntpd stop

     nasty
  */

  if ((pp = getenv ("RC_RUNSCRIPT_PID")))
    {
      if (sscanf (pp, "%d", &runscript_pid) != 1)
	runscript_pid = 0;
    }

  while ((entry = readdir (procdir)) != NULL)
    {
      if (sscanf (entry->d_name, "%d", &p) != 1)
	continue;
      foundany = true;

      if (runscript_pid != 0 && runscript_pid == p)
	continue;

      if (pid != 0 && pid != p)
	continue;

      if (uid)
	{
	  snprintf (buffer, sizeof (buffer), "/proc/%d", pid);
	  if (stat (buffer, &sb) != 0 || sb.st_uid != uid)
	    continue;
	}

      if (cmd && ! pid_is_cmd (p, cmd))
	continue;
     
      if (exec && ! cmd && ! pid_is_exec (p, exec))
	continue;

      pids = realloc (pids, sizeof (pid_t) * (npids + 2));
      if (! pids)
	eerrorx ("memory exhausted");

      pids[npids] = p;
      pids[npids + 1] = 0;
      npids++;
    }
  closedir (procdir);

  if (! foundany)
    eerrorx ("nothing in /proc");

  return (pids);
}

#elif defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)

# if defined(__FreeBSD__)
#  define _KINFO_PROC kinfo_proc
#  define _KVM_GETPROCS kvm_getprocs
#  define _KVM_GETARGV kvm_getargv
#  define _GET_KINFO_UID(kp) (kp.ki_ruid)
#  define _GET_KINFO_COMM(kp) (kp.ki_comm)
#  define _GET_KINFO_PID(kp) (kp.ki_pid)
# else
#  define _KINFO_PROC kinfo_proc2
#  define _KVM_GETPROCS kvm_getprocs2
#  define _KVM_GETARGV kvm_getargv2
#  define _GET_KINFO_UID(kp) (kp.p_ruid)
#  define _GET_KINFO_COMM(kp) (kp.p_comm)
#  define _GET_KINFO_PID(kp) (kp.p_pid)
# endif

pid_t *rc_find_pids (const char *exec, const char *cmd,
		     uid_t uid, pid_t pid)
{
  static kvm_t *kd = NULL;
  char errbuf[_POSIX2_LINE_MAX];
  struct _KINFO_PROC *kp;
  int i;
  int processes = 0;
  int argc = 0;
  char **argv;
  pid_t *pids = NULL;
  int npids = 0;

  if ((kd = kvm_openfiles (NULL, NULL, NULL, O_RDONLY, errbuf)) == NULL)
    eerrorx ("kvm_open: %s", errbuf);

  kp = _KVM_GETPROCS (kd, KERN_PROC_PROC, 0, &processes);
  for (i = 0; i < processes; i++)
    {
      pid_t p = _GET_KINFO_PID (kp[i]);
      if (pid != 0 && pid != p)
	continue;

      if (uid != 0 && uid != _GET_KINFO_UID (kp[i]))
	continue;

      if (cmd)
	{
	  if (! _GET_KINFO_COMM (kp[i]) ||
	      strcmp (cmd, _GET_KINFO_COMM (kp[i])) != 0)
	    continue;
	}

      if (exec && ! cmd)
	{
	  if ((argv = _KVM_GETARGV (kd, &kp[i], argc)) == NULL || ! *argv)
	    continue;

	  if (strcmp (*argv, exec) != 0) 
	    continue;
	}

      pids = realloc (pids, sizeof (pid_t) * (npids + 2));
      if (! pids)
	eerrorx ("memory exhausted");

      pids[npids] = p;
      pids[npids + 1] = 0;
      npids++;
    }
  kvm_close(kd);

  return (pids);
}

#else
#  error "Platform not supported!"
#endif

static bool _match_daemon (const char *path, const char *file,
			   const char *mexec, const char *mname,
			   const char *mpidfile)
{
  char buffer[RC_LINEBUFFER];
  char *ffile = rc_strcatpaths (path, file, NULL);
  FILE *fp;
  int lc = 0;
  int m = 0;

  if (! rc_exists (ffile))
    {
      free (ffile);
      return (false);
    }

  if ((fp = fopen (ffile, "r")) == NULL)
    {
      eerror ("fopen `%s': %s", ffile, strerror (errno));
      free (ffile);
      return (false);
    }

  if (! mname)
    m += 10;
  if (! mpidfile)
    m += 100;

  memset (buffer, 0, sizeof (buffer));
  while ((fgets (buffer, RC_LINEBUFFER, fp)))
    {
      int lb = strlen (buffer) - 1;
      if (buffer[lb] == '\n')
	buffer[lb] = 0;

      if (strcmp (buffer, mexec) == 0)
	m += 1;
      else if (mname && strcmp (buffer, mname) == 0)
	m += 10;
      else if (mpidfile && strcmp (buffer, mpidfile) == 0)
	m += 100;

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

void rc_set_service_daemon (const char *service, const char *exec,
			    const char *name, const char *pidfile,
			    bool started)
{
  char *dirpath = rc_strcatpaths (RC_SVCDIR, "daemons", basename (service), NULL);
  char **files = NULL;
  char *file;
  char *ffile = NULL;
  int i;
  char *mexec;
  char *mname;
  char *mpidfile;
  int nfiles = 0;

  if (! exec && ! name && ! pidfile)
    return;

  if (exec)
    {
      i = strlen (exec) + 6;
      mexec = rc_xmalloc (sizeof (char *) * i);
      snprintf (mexec, i, "exec=%s", exec);
    }
  else
    mexec = strdup ("exec=");

  if (name)
    {
      i = strlen (name) + 6;
      mname = rc_xmalloc (sizeof (char *) * i);
      snprintf (mname, i, "name=%s", name);
    }
  else
    mname = strdup ("name=");

  if (pidfile)
    {
      i = strlen (pidfile) + 9;
      mpidfile = rc_xmalloc (sizeof (char *) * i);
      snprintf (mpidfile, i, "pidfile=%s", pidfile);
    }
  else
    mpidfile = strdup ("pidfile=");

  /* Regardless, erase any existing daemon info */
  if (rc_is_dir (dirpath))
    {
      char *oldfile = NULL;
      files = rc_ls_dir (NULL, dirpath, 0);
      STRLIST_FOREACH (files, file, i)
	{
	  ffile = rc_strcatpaths (dirpath, file, NULL);
	  nfiles++;

	  if (! oldfile)
	    {
	      if (_match_daemon (dirpath, file, mexec, mname, mpidfile))
		{
		  unlink (ffile);
		  oldfile = ffile;
		  nfiles--;
		}
	    }
	  else
	    {
	      rename (ffile, oldfile);
	      free (oldfile);
	      oldfile = ffile;
	    }
	}
      if (ffile)
	free (ffile); 
      free (files);
    }

  /* Now store our daemon info */
  if (started)
    {
      char buffer[10];
      FILE *fp;

      if (! rc_is_dir (dirpath))
	if (mkdir (dirpath, 0755) != 0)
	  eerror ("mkdir `%s': %s", dirpath, strerror (errno));

      snprintf (buffer, sizeof (buffer), "%03d", nfiles + 1);
      file = rc_strcatpaths (dirpath, buffer, NULL);
      if ((fp = fopen (file, "w")) == NULL)
	eerror ("fopen `%s': %s", file, strerror (errno));
      else
	{
	  fprintf (fp, "%s\n%s\n%s\n", mexec, mname, mpidfile);
	  fclose (fp);
	}
      free (file);
    }

  free (mexec);
  free (mname);
  free (mpidfile);
  free (dirpath);
}

bool rc_service_started_daemon (const char *service, const char *exec,
				int indx)
{
  char *dirpath;
  char *file;
  int i;
  char *mexec;
  bool retval = false;

  if (! service || ! exec)
    return (false);

  dirpath = rc_strcatpaths (RC_SVCDIR, "daemons", basename (service), NULL);
  if (! rc_is_dir (dirpath))
    {
      free (dirpath);
      return (false);
    }

  i = strlen (exec) + 6;
  mexec = rc_xmalloc (sizeof (char *) * i);
  snprintf (mexec, i, "exec=%s", exec);

  if (indx > 0)
    {
      file = rc_xmalloc (sizeof (char *) * 10);
      snprintf (file, sizeof (file), "%03d", indx);
      retval = _match_daemon (dirpath, file, mexec, NULL, NULL);
      free (file);
    }
  else
    {
      char **files = rc_ls_dir (NULL, dirpath, 0);
      STRLIST_FOREACH (files, file, i)
	{
	  retval = _match_daemon (dirpath, file, mexec, NULL, NULL);
	  if (retval)
	    break;
	}
      free (files);
    }

  free (mexec);
  return (retval);
}

bool rc_service_daemons_crashed (const char *service)
{
  char *dirpath;
  char **files;
  char *file;
  char *path;
  int i;
  FILE *fp;
  char buffer[RC_LINEBUFFER];
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

  dirpath = rc_strcatpaths (RC_SVCDIR, "daemons", basename (service), NULL);
  if (! rc_is_dir (dirpath))
    {
      free (dirpath);
      return (false);
    }

  memset (buffer, 0, sizeof (buffer));
  files = rc_ls_dir (NULL, dirpath, 0);
  STRLIST_FOREACH (files, file, i)
    {
      path = rc_strcatpaths (dirpath, file, NULL);
      fp = fopen (path, "r");
      free (path);
      if (! fp)
	{
	  eerror ("fopen `%s': %s", file, strerror (errno));
	  continue;
	}

      while ((fgets (buffer, RC_LINEBUFFER, fp)))
	{
	  int lb = strlen (buffer) - 1;
	  if (buffer[lb] == '\n')
	    buffer[lb] = 0;

	  p = buffer;
	  if ((token = strsep (&p, "=")) == NULL || ! p)
	    continue;

	  if (strlen (p) == 0)
	    continue;

	  if (strcmp (token, "exec") == 0)
	    {
	      if (exec)
		free (exec);
	      exec = strdup (p);
	    }
	  else if (strcmp (token, "name") == 0)
	    {
	      if (name)
		free (name);
	      name = strdup (p);
	    }
	  else if (strcmp (token, "pidfile") == 0)
	    {
	      if (pidfile)
		free (pidfile);
	      pidfile = strdup (p);
	    }
	}
      fclose (fp);

      pid = 0;
      if (pidfile)
	{
	  if (! rc_exists (pidfile))
	    {
	      retval = true;
	      break;
	    }

	  if ((fp = fopen (pidfile, "r")) == NULL)
	    {
		eerror ("fopen `%s': %s", pidfile, strerror (errno));
		retval = true;
		break;
	    }

	  if (fscanf (fp, "%d", &pid) != 1)
	    {
	      eerror ("no pid found in `%s'", pidfile);
	      fclose (fp);
	      retval = true;
	      break;
	    }

	    fclose (fp);
	    free (pidfile);
	    pidfile = NULL;
	}

      if ((pids = rc_find_pids (exec, name, 0, pid)) == NULL)
	{
	  retval = true;
	  break;
	}
      free (pids);

      if (exec)
	{
	  free (exec);
	  exec = NULL;
	}
      if (name)
	{
	  free (name);
	  name = NULL;
	}
    }

  if (exec)
    {
      free (exec);
      exec = NULL;
    }
  if (name)
    {
      free (name);
      name = NULL;
    }

  free (dirpath);
  rc_strlist_free (files);

  return (retval);
}
