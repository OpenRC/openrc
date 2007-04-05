/*
   fstabinfo.c
   Gets information about /etc/fstab.

   Copyright 2007 Gentoo Foundation
   */

#include <errno.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Yay for linux and it's non liking of POSIX functions.
   Okay, we could use getfsent but the man page says use getmntent instead
   AND we don't have getfsent on uclibc or dietlibc for some odd reason. */
#ifdef __linux__
#define HAVE_GETMNTENT
#include <mntent.h>
#define GET_ENT getmntent (fp)
#define GET_ENT_FILE(_name) getmntfile (fp, _name)
#define END_ENT endmntent (fp)
#define ENT_DEVICE(_ent) ent->mnt_fsname
#define ENT_FILE(_ent) ent->mnt_dir
#define ENT_TYPE(_ent) ent->mnt_type
#define ENT_OPTS(_ent) ent->mnt_opts
#define ENT_PASS(_ent) ent->mnt_passno
#else
#define HAVE_GETFSENT
#include <fstab.h>
#define GET_ENT getfsent ()
#define GET_ENT_FILE(_name) getfsfile (_name)
#define END_ENT endfsent ()
#define ENT_DEVICE(_ent) ent->fs_spec
#define ENT_TYPE(_ent) ent->fs_vfstype
#define ENT_FILE(_ent) ent->fs_file
#define ENT_OPTS(_ent) ent->fs_mntops
#define ENT_PASS(_ent) ent->fs_passno
#endif

#include "einfo.h"

#ifdef HAVE_GETMNTENT
static struct mntent *getmntfile (FILE *fp, const char *file)
{
  struct mntent *ent;

  while ((ent = getmntent (fp)))
   if (strcmp (file, ent->mnt_dir) == 0)
     return (ent);

  return (NULL);
}
#endif

int main (int argc, char **argv)
{
  int i;
#ifdef HAVE_GETMNTENT
  FILE *fp;
  struct mntent *ent;
#else
  struct fstab *ent;
#endif
  int result = EXIT_FAILURE;
  char *p;
  char *token;
  int n = 0;

  for (i = 1; i < argc; i++)
    {
#ifdef HAVE_GETMNTENT
      fp = setmntent ("/etc/fstab", "r");
#endif

      if (strcmp (argv[i], "--fstype") == 0 && i + 1 < argc)
	{
	  i++;
	  p = argv[i];
	  while ((token = strsep (&p, ",")))
	    while ((ent = GET_ENT))
	     if (strcmp (token, ENT_TYPE (ent)) == 0)
	       printf ("%s\n", ENT_FILE (ent));
	  result = EXIT_SUCCESS;
	}

      if (strcmp (argv[i], "--mount-cmd") == 0 && i + 1 < argc)
	{
	  i++;
	  if ((ent = GET_ENT_FILE (argv[i])) == NULL)
	    continue;
	  printf ("-o %s -t %s %s %s\n", ENT_OPTS (ent), ENT_TYPE (ent),
		  ENT_DEVICE (ent), ENT_FILE (ent));
	  result = EXIT_SUCCESS;
	}

      if (strcmp (argv[i], "--opts") == 0 && i + 1 < argc)
	{
	  i++;
	  if ((ent = GET_ENT_FILE (argv[i])) == NULL)
	    continue;
	  printf ("%s\n", ENT_OPTS (ent));
	  result = EXIT_SUCCESS;
	}
	  
      if (strcmp (argv[i], "--passno") == 0 && i + 1 < argc)
	{
	  i++;
	  switch (argv[i][0])
	    {
	    case '=':
	    case '<':
	    case '>':
	      if (sscanf (argv[i] + 1, "%d", &n) != 1)
		eerrorx ("%s: invalid passno %s", argv[0], argv[i] + 1);

	      while ((ent = GET_ENT))
		{
		  if (((argv[i][0] == '=' && n == ENT_PASS (ent)) ||
		       (argv[i][0] == '<' && n > ENT_PASS (ent)) ||
		       (argv[i][0] == '>' && n < ENT_PASS (ent))) &&
		      strcmp (ENT_FILE (ent), "none") != 0)
		    printf ("%s\n", ENT_FILE (ent));
		}

	    default:
	      if ((ent = GET_ENT_FILE (argv[i])) == NULL)
		continue;
	      printf ("%d\n", ENT_PASS (ent));
	      result = EXIT_SUCCESS;
	    }
	}

      END_ENT;

      if (result != EXIT_SUCCESS)
	{
	  eerror ("%s: unknown option `%s'", basename (argv[0]), argv[i]);
	  break;
	}
 
    }

  exit (result);
}

