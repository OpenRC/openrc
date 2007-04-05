/*
   rc.h 
   Header file for external applications to get RC information.
   Copyright 2007 Gentoo Foundation
   Released under the GPLv2
   */

#ifndef __RC_H__
#define __RC_H__

#ifdef __GNUC__
#  define SENTINEL __attribute__ ((__sentinel__))
#endif

#include <sys/types.h>
#include <stdbool.h>

/* Special level names */
#define RC_LEVEL_SYSINIT	"sysinit"
#define RC_LEVEL_BOOT		"boot"
#define RC_LEVEL_SINGLE		"single"
#define RC_LEVEL_SHUTDOWN	"shutdown"
#define RC_LEVEL_REBOOT		"reboot"
#define RC_LEVEL_DEFAULT	"default"

typedef enum
{
  rc_service_started,
  rc_service_stopped,
  rc_service_starting,
  rc_service_stopping,
  rc_service_inactive,
  rc_service_wasinactive,
  rc_service_coldplugged,
  rc_service_failed,
  rc_service_scheduled,
  rc_service_crashed
} rc_service_state_t;

char *rc_resolve_service (const char *service);
bool rc_service_exists (const char *service);
bool rc_service_in_runlevel (const char *service, const char *runlevel);
bool rc_service_state (const char *service, rc_service_state_t state);
bool rc_mark_service (const char *service, rc_service_state_t state);
pid_t rc_stop_service (const char *service);
pid_t rc_start_service (const char *service);
void rc_schedule_start_service (const char *service,
				const char *service_to_start);
char **rc_services_scheduled_by (const char *service);
void rc_schedule_clear (const char *service);
bool rc_wait_service (const char *service);
bool rc_get_service_option (const char *service, const char *option,
			    char *value);
bool rc_set_service_option (const char *service, const char *option,
			    const char *value);
void rc_set_service_daemon (const char *service, const char *exec,
			    const char *name, const char *pidfile,
			    bool started);
bool rc_service_started_daemon (const char *service, const char *exec,
				int indx);

bool rc_allow_plug (char *service);

char *rc_get_runlevel (void);
void rc_set_runlevel (const char *runlevel);
bool rc_runlevel_exists (const char *runlevel);
char **rc_get_runlevels (void);
bool rc_runlevel_starting (void);
bool rc_runlevel_stopping (void);
bool rc_service_add (const char *runlevel, const char *service);
bool rc_service_delete (const char *runlevel, const char *service);
char **rc_services_in_runlevel (const char *runlevel);
char **rc_services_in_state (rc_service_state_t state);
char **rc_services_scheduled (const char *service);

/* Find pids based on criteria - free the pointer returned after use */
pid_t *rc_find_pids (const char *exec, const char *cmd,
		     uid_t uid, pid_t pid);
/* Checks that all daemons started with start-stop-daemon by the service
   are still running. If so, return false otherwise true.
   You should check that the service has been started before calling this. */
bool rc_service_daemons_crashed (const char *service);

/* Dependency tree structs and functions. */
typedef struct rc_deptype
{
  char *type;
  char **services;
  struct rc_deptype *next;
} rc_deptype_t;

typedef struct rc_depinfo
{
  char *service;
  rc_deptype_t *depends;
  struct rc_depinfo *next;
} rc_depinfo_t;


/* Options for rc_dep_depends and rc_order_services.
   When changing runlevels, you should use RC_DEP_START and RC_DEP_STOP for
   the start and stop lists as we tweak the provided services for this. */
#define RC_DEP_TRACE  	0x01
#define RC_DEP_STRICT	0x02
#define RC_DEP_START    0x04
#define RC_DEP_STOP     0x08

int rc_update_deptree (bool force);
rc_depinfo_t *rc_load_deptree (void);
rc_depinfo_t *rc_get_depinfo (rc_depinfo_t *deptree, const char *service);
rc_deptype_t *rc_get_deptype (rc_depinfo_t *depinfo, const char *type);
char **rc_get_depends (rc_depinfo_t *deptree, char **types,
		       char **services, const char *runlevel, int options);
/* List all the services that should be started, in order, the the
   given runlevel, including sysinit and boot services where
   approriate.
   If reboot, shutdown or single are given then we list all the services
   we that we need to shutdown in order. */
char **rc_order_services (rc_depinfo_t *deptree, const char *runlevel,
			  int options);

void rc_free_deptree (rc_depinfo_t *deptree);

/* Plugin handler
   For each plugin loaded we will call it's _name_hook with the below
   enum and either the runlevel name or service name. For example
   int _splash_hook (rc_hook_t hook, const char *name);
   Plugins are called when rc does something. This does not indicate an
   end result and the plugin should use the above functions to query things
   like service status. */
typedef enum
{
  rc_hook_runlevel_stop_in = 1,
  rc_hook_runlevel_stop_out,
  rc_hook_runlevel_start_in,
  rc_hook_runlevel_start_out,
  rc_hook_service_stop_in,
  rc_hook_service_stop_out,
  rc_hook_service_start_in,
  rc_hook_service_start_out
} rc_hook_t;

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
char **rc_strlist_addsort (char **list, const char *item);
char **rc_strlist_addsortc (char **list, const char *item);
char **rc_strlist_delete (char **list, const char *item);
void rc_strlist_reverse (char **list);
void rc_strlist_free (char **list);

#endif
