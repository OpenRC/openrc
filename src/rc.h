/*!
 * @file rc.h 
 * @brief Describes how to interface with the RC library
 * @internal
 *
 * Copyright 2007 Gentoo Foundation
 * Released under the GPLv2
 */

#ifndef __RC_H__
#define __RC_H__

#ifdef __GNUC__
#  define GCC_VERSION (__GNUC__ * 1000 + __GNUC__MINOR)
#  if (GCC_VERSION >= 3005)
#    define SENTINEL __attribute__ ((__sentinel__))
#  endif
#endif
#ifndef SENTINEL
#  define SENTINEL
#endif

#include <sys/types.h>
#include <stdbool.h>
//#include <stdio.h>

/*! @name Reserved runlevel names */
#define RC_LEVEL_SYSINIT    "sysinit"
#define RC_LEVEL_SINGLE     "single"
#define RC_LEVEL_SHUTDOWN   "shutdown"
#define RC_LEVEL_REBOOT     "reboot"

/*! Return the current runlevel.
 * @return the current runlevel */
char *rc_runlevel_get (void);

/*! Checks if the runlevel exists or not
 * @param runlevel to check
 * @return true if the runlevel exists, otherwise false */
bool rc_runlevel_exists (const char *runlevel);

/*! Return a NULL terminated list of runlevels
 * @return a NULL terminated list of runlevels */
char **rc_runlevel_list (void);

/*! Set the runlevel.
 * This just changes the stored runlevel and does not start or stop any services.
 * @param runlevel to store */
bool rc_runlevel_set (const char *runlevel);

/*! Is the runlevel starting?
 * @return true if yes, otherwise false */
bool rc_runlevel_starting (void);

/*! Is the runlevel stopping?
 * @return true if yes, otherwise false */
bool rc_runlevel_stopping (void);

/*! @name RC
 * A service can be given as a full path or just its name.
 * If its just a name then we try to resolve the service to a full path.
 * This should allow the use if local init.d directories in the future. */

/*! @brief States a service can be in */
typedef enum
{
	/* These are actual states
	 * The service has to be in one only at all times */
	RC_SERVICE_STOPPED     = 0x0001,
	RC_SERVICE_STARTED     = 0x0002,
	RC_SERVICE_STOPPING    = 0x0004,
	RC_SERVICE_STARTING    = 0x0008,
	RC_SERVICE_INACTIVE    = 0x0010,

	/* Service may or may not have been coldplugged */
	RC_SERVICE_COLDPLUGGED = 0x0100,

	/* Optional states service could also be in */
	RC_SERVICE_FAILED      = 0x0200,
	RC_SERVICE_SCHEDULED   = 0x0400,
	RC_SERVICE_WASINACTIVE = 0x0800,
} rc_service_state_t;

/*! Add the service to the runlevel
 * @param runlevel to add to
 * @param service to add
 * @return true if successful, otherwise false */
bool rc_service_add (const char *runlevel, const char *service);

/*! Remove the service from the runlevel
 * @param runlevel to remove from
 * @param service to remove
 * @return true if sucessful, otherwise false */
bool rc_service_delete (const char *runlevel, const char *service);

/*! Save the arguments to find a running daemon
 * @param service to save arguments for
 * @param exec that we started
 * @param name of the process (optional)
 * @param pidfile of the process (optional)
 * @param started if true, add the arguments otherwise remove existing matching arguments */
void rc_service_daemon_set (const char *service, const char *exec,
							const char *name, const char *pidfile,
							bool started);

/*! Returns a description of what the service and/or option does.
 * @param service to check
 * @param option to check (if NULL, service description)
 * @return a newly allocated pointer to the description */
char *rc_service_description (const char *service, const char *option);

/*! Checks if a service exists or not.
 * @param service to check
 * @return true if service exists, otherwise false */
bool rc_service_exists (const char *service);

/*! Checks if a service is in a runlevel
 * @param service to check
 * @param runlevel it should be in
 * @return true if service is in the runlevel, otherwise false */
bool rc_service_in_runlevel (const char *service, const char *runlevel);

/*! Marks the service state
 * @param service to mark
 * @param state service should be in
 * @return true if service state change was successful, otherwise false */
bool rc_service_mark (const char *service, rc_service_state_t state);

/*! Lists the extra options a service has
 * @param service to load the options from
 * @return NULL terminated string list of options */
char **rc_service_options (const char *service);

/*! Check if the service is allowed to be hot/cold plugged
 * @param service to check
 * @return true if allowed, otherwise false */
bool rc_service_plugable (char *service);

/*! Resolves a service name to its full path.
 * @param service to check
 * @return pointer to full path of service */
char *rc_service_resolve (const char *service);

/*! Schedule a service to be started when another service starts
 * @param service that starts the scheduled service when started
 * @param service_to_start service that will be started */
bool rc_service_schedule_start (const char *service,
								const char *service_to_start);
/*! Return a NULL terminated list of services that are scheduled to start
 * when the given service has started
 * @param service to check
 * @return NULL terminated list of services scheduled to start */
char **rc_services_scheduled_by (const char *service);

/*! Clear the list of services scheduled to be started by this service
 * @param service to clear */
void rc_service_schedule_clear (const char *service);

/*! Checks if a service in in a state
 * @param service to check
 * @return state of the service */
rc_service_state_t rc_service_state (const char *service);

/*! Start a service
 * @param service to start
 * @return pid of the service starting process */
pid_t rc_service_start (const char *service);

/*! Stop a service
 * @param service to stop
 * @return pid of service stopping process */
pid_t rc_service_stop (const char *service);

/*! Check if the service started the daemon
 * @param service to check
 * @param exec to check
 * @param indx of the daemon (optional - 1st daemon, 2nd daemon, etc)
 * @return true if started by this service, otherwise false */
bool rc_service_started_daemon (const char *service, const char *exec,
								int indx);

/*! Return a saved value for a service
 * @param service to check
 * @param option to load
 * @return saved value */
char *rc_service_value_get (const char *service, const char *option);

/*! Save a persistent value for a service
 * @param service to save for
 * @param option to save
 * @param value of the option
 * @return true if saved, otherwise false */
bool rc_service_value_set (const char *service, const char *option,
							const char *value);

/*! Wait for a service to finish
 * @param service to wait for
 * @return true if service finished before timeout, otherwise false */
bool rc_service_wait (const char *service);

/*! List the services in a runlevel
 * @param runlevel to list
 * @return NULL terminated list of services */
char **rc_services_in_runlevel (const char *runlevel);

/*! List the services in a state
 * @param state to list
 * @return NULL terminated list of services */
char **rc_services_in_state (rc_service_state_t state);

/*! List the services shceduled to start when this one does
 * @param service to check
 * @return  NULL terminated list of services */
char **rc_services_scheduled (const char *service);

/*! Checks that all daemons started with start-stop-daemon by the service
 * are still running.
 * @param service to check
 * @return true if all daemons started are still running, otherwise false */
bool rc_service_daemons_crashed (const char *service);

/*! Wait for a process to finish
 * @param pid to wait for
 * @return exit status of the process */
int rc_waitpid (pid_t pid); 

/*! Find processes based on criteria.
 * All of these are optional.
 * pid overrides anything else.
 * If both exec and cmd are given then we ignore exec.
 * @param exec to check for
 * @param cmd to check for
 * @param uid to check for
 * @param pid to check for
 * @return NULL terminated list of pids */
pid_t *rc_find_pids (const char *exec, const char *cmd,
					 uid_t uid, pid_t pid);

/*! @name Dependency options
 * These options can change the services found by the rc_get_depinfo and
 * rc_get_depends functions. */
/*! Trace provided services */
#define RC_DEP_TRACE    0x01
/*! Only use services added to runlevels */
#define RC_DEP_STRICT   0x02
/*! Runlevel is starting */
#define RC_DEP_START    0x04
/*! Runlevel is stopping */
#define RC_DEP_STOP     0x08

/*! @name Dependencies
 * We analyse each init script and cache the resultant dependency tree.
 * This tree can be accessed using the below functions. */ 

#ifndef _IN_LIBRC
/* Handles to internal structures */
typedef void *rc_depinfo_t;
#endif

/*! Update the cached dependency tree if it's older than any init script,
 * its configuration file or an external configuration file the init script
 * has specified.
 * @return true if successful, otherwise false */
bool rc_deptree_update (void);

/*! Check if the cached dependency tree is older than any init script,
 * its configuration file or an external configuration file the init script
 * has specified.
 * @return true if it needs updating, otherwise false */
bool rc_deptree_update_needed (void);

/*! Load the cached dependency tree and return a pointer to it.
 * This pointer should be freed with rc_deptree_free when done.
 * @return pointer to the dependency tree */
rc_depinfo_t *rc_deptree_load (void);

/*! List all the services in order that the given services have
 * for the given types and options.
 * @param deptree to search
 * @param types to use (ineed, iuse, etc) 
 * @param services to check
 * @param options to pass
 * @return NULL terminated list of services in order */
char **rc_deptree_depends (rc_depinfo_t *deptree, char **types,
						   char **services, const char *runlevel, int options);

/*! List all the services that should be stoppned and then started, in order,
 * for the given runlevel, including sysinit and boot services where
 * approriate.
 * @param deptree to search
 * @param runlevel to change into
 * @param options to pass
 * @return NULL terminated list of services in order */
char **rc_deptree_order_services (rc_depinfo_t *deptree, const char *runlevel,
								  int options);

/*! Free a deptree and its information
 * @param deptree to free */
void rc_deptree_free (rc_depinfo_t *deptree);

/*! @name Plugins
 * For each plugin loaded we will call rc_plugin_hook with the below
 * enum and either the runlevel name or service name.
 *
 * Plugins are called when rc does something. This does not indicate an
 * end result and the plugin should use the above functions to query things
 * like service status.
 *
 * The service hooks have extra ones - now and done. This is because after
 * start_in we may start other services before we start the service in
 * question. now shows we really will start the service now and done shows
 * when we have done it as may start scheduled services at this point. */
/*! Points at which a plugin can hook into RC */
typedef enum
{
	RC_HOOK_RUNLEVEL_STOP_IN   = 1,
	RC_HOOK_RUNLEVEL_STOP_OUT  = 4,
	RC_HOOK_RUNLEVEL_START_IN  = 5,
	RC_HOOK_RUNLEVEL_START_OUT = 8,
	/*! We send the abort if an init script requests we abort and drop
	 * into single user mode if system not fully booted */
	RC_HOOK_ABORT              = 99,
	RC_HOOK_SERVICE_STOP_IN    = 101,
	RC_HOOK_SERVICE_STOP_NOW   = 102,
	RC_HOOK_SERVICE_STOP_DONE  = 103,
	RC_HOOK_SERVICE_STOP_OUT   = 104,
	RC_HOOK_SERVICE_START_IN   = 105,
	RC_HOOK_SERVICE_START_NOW  = 106,
	RC_HOOK_SERVICE_START_DONE = 107,
	RC_HOOK_SERVICE_START_OUT  = 108
} rc_hook_t;

/*! Plugin entry point
 * @param hook point
 * @param name of runlevel or service
 * @return 0 for success otherwise -1 */
int rc_plugin_hook (rc_hook_t hook, const char *name);

/*! Plugins should write FOO=BAR to this fd to set any environment
 * variables they wish. Variables should be separated by NULLs. */
extern FILE *rc_environ_fd;

/*! @name Configuration */
/*! Return a NULL terminated list of non comment lines from a file. */
char **rc_config_list (const char *file);

/*! Return a NULL terminated list of key=value lines from a file. */
char **rc_config_load (const char *file);

/*! Return the value of the entry from a key=value list. */
char *rc_config_value (char **list, const char *entry);

/*! Return a NULL terminated string list of variables allowed through
 * from the current environemnt. */
char **rc_env_filter (void);

/*! Return a NULL terminated string list of enviroment variables made from
 * our configuration files. */
char **rc_env_config (void);

/*! @name String List functions
 * Handy functions for dealing with string arrays of char **.
 * It's safe to assume that any function here that uses char ** is a string
 * list that can be manipulated with the below functions. Every string list
 * should be released with a call to rc_strlist_free.*/

/*! Duplicate the item, add it to end of the list and return a pointer to it.
 * @param list to add the item too
 * @param item to add.
 * @return pointer to newly added item */
char *rc_strlist_add (char ***list, const char *item);

/*! If the item does not exist in the list, duplicate it, add it to the
 * list and then return a pointer to it.
 * @param list to add the item too
 * @param item to add.
 * @return pointer to newly added item */
char *rc_strlist_addu (char ***list, const char *item);

/*! Duplicate the item, add it to the list at the point based on locale and
 * then return a pointer to it.
 * @param list to add the item too
 * @param item to add.
 * @return pointer to newly added item */
char *rc_strlist_addsort (char ***list, const char *item);

/*! Duplicate the item, add it to the list at the point based on C locale and
 * then return a pointer to it.
 * @param list to add the item too
 * @param item to add.
 * @return pointer to newly added item */
char *rc_strlist_addsortc (char ***list, const char *item);

/*! If the item does not exist in the list, duplicate it, add it to the
 * list based on locale and then return a pointer to it.
 * @param list to add the item too
 * @param item to add.
 * @return pointer to newly added item */
char *rc_strlist_addsortu (char ***list, const char *item);

/*! Free the item and remove it from the list. Return 0 on success otherwise -1.
 * @param list to add the item too
 * @param item to add.
 * @return true on success, otherwise false */
bool rc_strlist_delete (char ***list, const char *item);

/*! Moves the contents of list2 onto list1, so list2 is effectively emptied.
 * Returns a pointer to the last item on the new list.
 * @param list1 to append to
 * @param list2 to move from
 * @return pointer to the last item on the list */
char *rc_strlist_join (char ***list1, char **list2);

/*! Reverses the contents of the list.
 * @param list to reverse */
void rc_strlist_reverse (char **list);

/*! Frees each item on the list and the list itself.
 * @param list to free */
void rc_strlist_free (char **list);

/*! @name Memory Allocation
 * Ensure that if we cannot allocate the memory then we exit */
/*@{*/

/*! Allocate a block of memory
 * @param size of memory to allocate
 * @return pointer to memory */
void *rc_xmalloc (size_t size);

/*! Re-size a block of memory
 * @param ptr to the block of memory to re-size
 * @param size memory should be
 * @return pointer to memory block */
void *rc_xrealloc (void *ptr, size_t size);

/*! Duplicate a NULL terminated string
 * @param str to duplicate
 * @return pointer to the new string */
char *rc_xstrdup (const char *str);
/*@}*/

/*! @name Utility
 * Although not RC specific functions, they are used by the supporting
 * applications */

/*! Concatenate paths adding '/' if needed. The resultant pointer should be
 * freed when finished with.
 * @param path1 starting path
 * @param paths NULL terminated list of paths to add
 * @return pointer to the new path */
char *rc_strcatpaths (const char *path1, const char *paths, ...) SENTINEL;

/*! Check if an environment variable is a boolean and return it's value.
 * If variable is not a boolean then we set errno to be ENOENT when it does
 * not exist or EINVAL if it's not a boolean.
 * @param variable to check
 * @return true if it matches true, yes or 1, false if otherwise. */
bool rc_env_bool (const char *variable);

/*! @name rc_ls_dir options */
/*! Ensure that an init.d service exists for each file returned */
#define RC_LS_INITD	0x01
#define RC_LS_DIR   0x02

/*! Return a NULL terminted sorted list of the contents of the directory
 * @param dir to list
 * @param options any options to apply
 * @return NULL terminated list */
char **rc_ls_dir (const char *dir, int options);

/*! Remove a directory
 * @param pathname to remove
 * @param top remove the top level directory too
 * @return true if successful, otherwise false */
bool rc_rm_dir (const char *pathname, bool top);

#endif
