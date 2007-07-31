/*
   rc-misc.h
   This is private to us and not for user consumption
   Copyright 2007 Gentoo Foundation
   */

#ifndef __RC_MISC_H__
#define __RC_MISC_H__

#ifndef LIB
#  define LIB "lib"
#endif

#define RC_LIBDIR	"/" LIB "/rcscripts/"
#define RC_SVCDIR	RC_LIBDIR "init.d/"
#define RC_DEPTREE 	RC_SVCDIR "deptree"
#define RC_RUNLEVELDIR 	"/etc/runlevels/"
#define RC_INITDIR	"/etc/init.d/"
#define RC_CONFDIR	"/etc/conf.d/"

#define RC_SVCDIR_STARTING	RC_SVCDIR "starting/"
#define RC_SVCDIR_INACTIVE	RC_SVCDIR "inactive/"
#define RC_SVCDIR_STARTED	RC_SVCDIR "started/"
#define RC_SVCDIR_COLDPLUGGED	RC_SVCDIR "coldplugged/"

#define RC_PLUGINDIR	RC_LIBDIR "plugins/"

/* Max buffer to read a line from a file */
#define RC_LINEBUFFER	4096 

/* Good defaults just incase user has none set */
#define RC_NET_FS_LIST_DEFAULT "afs cifs coda davfs fuse gfs ncpfs nfs nfs4 ocfs2 shfs smbfs"

/* RC utility functions.
   Although not directly related to RC in general, they are used by RC
   itself and the supporting applications. */
void *rc_xcalloc (size_t n, size_t size);
void *rc_xmalloc (size_t size);
void *rc_xrealloc (void *ptr, size_t size);
char *rc_xstrdup (const char *str);

/* Concat paths adding '/' if needed. */
char *rc_strcatpaths (const char *path1, const char *paths, ...) SENTINEL;

bool rc_is_env (const char *variable, const char *value);
bool rc_exists (const char *pathname);
bool rc_is_file (const char *pathname);
bool rc_is_link (const char *pathname);
bool rc_is_dir (const char *pathname);
bool rc_is_exec (const char *pathname);

#define RC_LS_INITD	0x01
char **rc_ls_dir (char **list, const char *dir, int options);

bool rc_rm_dir (const char *pathname, bool top);

/* Config file functions */
char **rc_get_list (char **list, const char *file);
char **rc_get_config (char **list, const char *file);
char *rc_get_config_entry (char **list, const char *entry);

/* Make an environment list which filters out all unwanted values
   and loads it up with our RC config */
char **rc_filter_env (void);
char **rc_config_env (char **env);

/* Handy functions for dealing with string arrays of char ** */
char **rc_strlist_add (char **list, const char *item);
char **rc_strlist_addu (char **list, const char *item);
char **rc_strlist_addsort (char **list, const char *item);
char **rc_strlist_addsortc (char **list, const char *item);
char **rc_strlist_addsortu (char **list, const char *item);
char **rc_strlist_delete (char **list, const char *item);
char **rc_strlist_join (char **list1, char **list2);
void rc_strlist_reverse (char **list);
void rc_strlist_free (char **list);

#endif
