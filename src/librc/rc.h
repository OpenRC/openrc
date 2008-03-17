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

#ifndef __RC_H__
#define __RC_H__

#ifdef __GNUC__
#  define GCC_VERSION (__GNUC__ * 1000 + __GNUC__MINOR)
#  if (GCC_VERSION >= 3005)
#    define SENTINEL __attribute__ ((__sentinel__))
#  endif
#  define DEPRECATED __attribute__ ((deprecated))
#endif
#ifndef SENTINEL
#  define SENTINEL
#endif

#include <sys/types.h>
#include <sys/queue.h>
#include <stdbool.h>
#include <stdio.h>

/* A doubly linked list using queue(3) for ease of use */
typedef struct rc_string {
	char *value;
	TAILQ_ENTRY(rc_string) entries;
} RC_STRING;
typedef TAILQ_HEAD(rc_stringlist, rc_string) RC_STRINGLIST;

/*! @name Reserved runlevel names */
#define RC_LEVEL_SYSINIT    "sysinit"
#define RC_LEVEL_SINGLE     "single"
#define RC_LEVEL_SHUTDOWN   "shutdown"
#define RC_LEVEL_REBOOT     "reboot"

/*! Return the current runlevel.
 * @return the current runlevel */
char *rc_runlevel_get(void);

/*! Checks if the runlevel exists or not
 * @param runlevel to check
 * @return true if the runlevel exists, otherwise false */
bool rc_runlevel_exists(const char *);

/*! Return a NULL terminated list of runlevels
 * @return a NULL terminated list of runlevels */
RC_STRINGLIST *rc_runlevel_list(void);

/*! Set the runlevel.
 * This just changes the stored runlevel and does not start or stop any
 * services.
 * @param runlevel to store */
bool rc_runlevel_set(const char *);

/*! Is the runlevel starting?
 * @return true if yes, otherwise false */
bool rc_runlevel_starting(void);

/*! Is the runlevel stopping?
 * @return true if yes, otherwise false */
bool rc_runlevel_stopping(void);

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
	RC_SERVICE_WASINACTIVE = 0x0800
} RC_SERVICE;

/*! Add the service to the runlevel
 * @param runlevel to add to
 * @param service to add
 * @return true if successful, otherwise false */
bool rc_service_add(const char *, const char *);

/*! Remove the service from the runlevel
 * @param runlevel to remove from
 * @param service to remove
 * @return true if sucessful, otherwise false */
bool rc_service_delete(const char *, const char *);

/*! Save the arguments to find a running daemon
 * @param service to save arguments for
 * @param exec that we started
 * @param name of the process (optional)
 * @param pidfile of the process (optional)
 * @param started if true, add the arguments otherwise remove existing matching arguments */
bool rc_service_daemon_set(const char *, const char *const *, const char *, const char *,
			   bool);

/*! Returns a description of what the service and/or option does.
 * @param service to check
 * @param option to check (if NULL, service description)
 * @return a newly allocated pointer to the description */
char *rc_service_description(const char *, const char *);

/*! Checks if a service exists or not.
 * @param service to check
 * @return true if service exists, otherwise false */
bool rc_service_exists(const char *);

/*! Checks if a service is in a runlevel
 * @param service to check
 * @param runlevel it should be in
 * @return true if service is in the runlevel, otherwise false */
bool rc_service_in_runlevel(const char *, const char *);

/*! Marks the service state
 * @param service to mark
 * @param state service should be in
 * @return true if service state change was successful, otherwise false */
bool rc_service_mark(const char *, RC_SERVICE);

/*! Lists the extra commands a service has
 * @param service to load the commands from
 * @return NULL terminated string list of commands */
RC_STRINGLIST *rc_service_extra_commands(const char *);

/*! Resolves a service name to its full path.
 * @param service to check
 * @return pointer to full path of service */
char *rc_service_resolve(const char *);

/*! Schedule a service to be started when another service starts
 * @param service that starts the scheduled service when started
 * @param service_to_start service that will be started */
bool rc_service_schedule_start(const char *, const char *);

/*! Return a NULL terminated list of services that are scheduled to start
 * when the given service has started
 * @param service to check
 * @return NULL terminated list of services scheduled to start */
RC_STRINGLIST *rc_services_scheduled_by(const char *);

/*! Clear the list of services scheduled to be started by this service
 * @param service to clear
 * @return true if no errors, otherwise false */
bool rc_service_schedule_clear(const char *);

/*! Checks if a service in in a state
 * @param service to check
 * @return state of the service */
RC_SERVICE rc_service_state(const char *);

/*! Start a service
 * @param service to start
 * @return pid of the service starting process */
pid_t rc_service_start(const char *);

/*! Stop a service
 * @param service to stop
 * @return pid of service stopping process */
pid_t rc_service_stop(const char *);

/*! Check if the service started the daemon
 * @param service to check
 * @param exec to check
 * @param indx of the daemon (optional - 1st daemon, 2nd daemon, etc)
 * @return true if started by this service, otherwise false */
bool rc_service_started_daemon(const char *, const char *const *, int);

/*! Return a saved value for a service
 * @param service to check
 * @param option to load
 * @return saved value */
char *rc_service_value_get(const char *, const char *);

/*! Save a persistent value for a service
 * @param service to save for
 * @param option to save
 * @param value of the option
 * @return true if saved, otherwise false */
bool rc_service_value_set(const char *, const char *, const char *);

/*! List the services in a runlevel
 * @param runlevel to list
 * @return NULL terminated list of services */
RC_STRINGLIST *rc_services_in_runlevel(const char *);

/*! List the services in a state
 * @param state to list
 * @return NULL terminated list of services */
RC_STRINGLIST *rc_services_in_state(RC_SERVICE);

/*! List the services shceduled to start when this one does
 * @param service to check
 * @return  NULL terminated list of services */
RC_STRINGLIST *rc_services_scheduled(const char *);

/*! Checks that all daemons started with start-stop-daemon by the service
 * are still running.
 * @param service to check
 * @return true if all daemons started are still running, otherwise false */
bool rc_service_daemons_crashed(const char *);

/*! @name System types
 * OpenRC can support some special sub system types, normally virtualization.
 * Some services cannot work in these systems, or we do something else. */
#define RC_SYS_JAIL    "JAIL"
#define RC_SYS_OPENVZ  "OPENVZ"
#define RC_SYS_PREFIX  "PREFIX"
#define RC_SYS_UML     "UML"
#define RC_SYS_VSERVER "VSERVER"
#define RC_SYS_XEN0    "XEN0"
#define RC_SYS_XENU    "XENU"
const char *rc_sys(void);

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

#ifdef _IN_LIBRC
/*! @name Dependency structures
 * private to librc */

/*! Singly linked list of dependency types that list the services the
 * type is for */
typedef struct rc_deptype
{
	/*! ineed, iuse, iafter, etc */
	char *type;
	/*! list of services */
	RC_STRINGLIST *services;
	/*! list of types */
	STAILQ_ENTRY(rc_deptype) entries;
} RC_DEPTYPE;

/*! Singly linked list of services and their dependencies */
typedef struct rc_depinfo
{
	/*! Name of service */
	char *service;
	/*! Dependencies */
	STAILQ_HEAD(, rc_deptype) depends;
	/*! List of entries */
	STAILQ_ENTRY(rc_depinfo) entries;
} RC_DEPINFO;

typedef STAILQ_HEAD(,rc_depinfo) RC_DEPTREE;
#else
/* Handles to internal structures */
typedef void *RC_DEPTREE;
#endif

/*! Check to see if source is newer than target.
 * If target is a directory then we traverse it and it's children.
* @return true if source is newer than target, otherwise false */ 
bool rc_newer_than(const char *, const char *);

/*! Update the cached dependency tree if it's older than any init script,
 * its configuration file or an external configuration file the init script
 * has specified.
 * @return true if successful, otherwise false */
bool rc_deptree_update(void);

/*! Check if the cached dependency tree is older than any init script,
 * its configuration file or an external configuration file the init script
 * has specified.
 * @return true if it needs updating, otherwise false */
bool rc_deptree_update_needed(void);

/*! Load the cached dependency tree and return a pointer to it.
 * This pointer should be freed with rc_deptree_free when done.
 * @return pointer to the dependency tree */
RC_DEPTREE *rc_deptree_load(void);

/*! List the depend for the type of service
 * @param deptree to search
 * @param type to use (keywords, etc)
 * @param service to check
 * @return NULL terminated list of services in order */
RC_STRINGLIST *rc_deptree_depend(const RC_DEPTREE *, const char *, const char *);

/*! List all the services in order that the given services have
 * for the given types and options.
 * @param deptree to search
 * @param types to use (ineed, iuse, etc)
 * @param services to check
 * @param options to pass
 * @return NULL terminated list of services in order */
RC_STRINGLIST *rc_deptree_depends(const RC_DEPTREE *, const RC_STRINGLIST *,
				  const RC_STRINGLIST *, const char *, int);

/*! List all the services that should be stoppned and then started, in order,
 * for the given runlevel, including sysinit and boot services where
 * approriate.
 * @param deptree to search
 * @param runlevel to change into
 * @param options to pass
 * @return NULL terminated list of services in order */
RC_STRINGLIST *rc_deptree_order(const RC_DEPTREE *, const char *, int);

/*! Free a deptree and its information
 * @param deptree to free */
void rc_deptree_free(RC_DEPTREE *);

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
} RC_HOOK;

/*! Plugin entry point
 * @param hook point
 * @param name of runlevel or service
 * @return 0 for success otherwise -1 */
int rc_plugin_hook(RC_HOOK, const char *);

/*! Plugins should write FOO=BAR to this fd to set any environment
 * variables they wish. Variables should be separated by NULLs. */
extern FILE *rc_environ_fd;

/*! @name Configuration
 * These functions help to deal with shell based configuration files */
/*! Return a line from a file, stripping the trailing newline. */
char *rc_getline(FILE *);

/*! Return a NULL terminated list of non comment lines from a file. */
RC_STRINGLIST *rc_config_list(const char *);

/*! Return a NULL terminated list of key=value lines from a file. */
RC_STRINGLIST *rc_config_load(const char *);

/*! Return the value of the entry from a key=value list. */
char *rc_config_value(RC_STRINGLIST *, const char *);

/*! Check if a variable is a boolean and return it's value.
 * If variable is not a boolean then we set errno to be ENOENT when it does
 * not exist or EINVAL if it's not a boolean.
 * @param variable to check
 * @return true if it matches true, yes or 1, false if otherwise. */
bool rc_yesno(const char *);

/*! @name String List functions
 * Every string list should be released with a call to rc_stringlist_free. */

/*! Create a new stringlinst
 * @return pointer to new list */
RC_STRINGLIST *rc_stringlist_new(void);

/*! Duplicate the item, add it to end of the list and return a pointer to it.
 * @param list to add the item too
 * @param item to add.
 * @return pointer to newly added item */
RC_STRING *rc_stringlist_add(RC_STRINGLIST *, const char *);

/*! If the item does not exist in the list, duplicate it, add it to the
 * list and then return a pointer to it.
 * @param list to add the item too
 * @param item to add.
 * @return pointer to newly added item */
RC_STRING *rc_stringlist_addu(RC_STRINGLIST *, const char *);

/*! Free the item and remove it from the list. Return 0 on success otherwise -1.
 * @param list to add the item too
 * @param item to add.
 * @return true on success, otherwise false */
bool rc_stringlist_delete(RC_STRINGLIST *, const char *);

/*! Sort the list according to C locale 
 * @param list to sort */
void rc_stringlist_sort(RC_STRINGLIST **);

/*! Frees each item on the list and the list itself.
 * @param list to free */
void rc_stringlist_free(RC_STRINGLIST *);

/*! Concatenate paths adding '/' if needed. The resultant pointer should be
 * freed when finished with.
 * @param path1 starting path
 * @param paths NULL terminated list of paths to add
 * @return pointer to the new path */
char *rc_strcatpaths(const char *, const char *, ...) SENTINEL;

typedef struct rc_pid
{
	pid_t pid;
	LIST_ENTRY(rc_pid) entries;
} RC_PID;
typedef LIST_HEAD(rc_pidlist, rc_pid) RC_PIDLIST;

/*! Find processes based on criteria.
 * All of these are optional.
 * pid overrides anything else.
 * If both exec and cmd are given then we ignore exec.
 * @param exec to check for
 * @param cmd to check for
 * @param uid to check for
 * @param pid to check for
 * @return NULL terminated list of pids */
RC_PIDLIST *rc_find_pids(const char *const *, const char *, uid_t, pid_t);

#endif
