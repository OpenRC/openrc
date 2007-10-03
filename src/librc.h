/*
 * librc.h
 * Internal header file to setup build env for files in librc.so
 * Copyright 2007 Gentoo Foundation
 * Released under the GPLv2
 */

#ifndef _LIBRC_H_
#define _LIBRC_H_

#define _IN_LIBRC

#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <sys/wait.h>

#ifdef __linux__
#include <sys/sysinfo.h>
#endif

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <limits.h>
#include <regex.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#if defined(__DragonFly__) || defined(__FreeBSD__) || \
	defined(__NetBSD__) || defined (__OpenBSD__)
#include <sys/param.h>
#include <sys/user.h>
#include <sys/sysctl.h>
#include <kvm.h>
#endif

#include "librc-depend.h"
#include "rc.h"
#include "rc-misc.h"
#include "strlist.h"

#include "hidden-visibility.h"
#define librc_hidden_proto(x) hidden_proto(x)
#define librc_hidden_def(x) hidden_def(x)

librc_hidden_proto(rc_deptree_depends)
librc_hidden_proto(rc_deptree_depinfo)
librc_hidden_proto(rc_deptree_deptype)
librc_hidden_proto(rc_deptree_free)
librc_hidden_proto(rc_deptree_load)
librc_hidden_proto(rc_deptree_order_services)
librc_hidden_proto(rc_deptree_update)
librc_hidden_proto(rc_deptree_update_needed)
librc_hidden_proto(rc_env_bool)
librc_hidden_proto(rc_exists)
librc_hidden_proto(rc_filter_env)
librc_hidden_proto(rc_find_pids)
librc_hidden_proto(rc_get_config)
librc_hidden_proto(rc_get_config_entry)
librc_hidden_proto(rc_get_list)
librc_hidden_proto(rc_is_dir)
librc_hidden_proto(rc_is_exec)
librc_hidden_proto(rc_is_file)
librc_hidden_proto(rc_is_link)
librc_hidden_proto(rc_ls_dir)
librc_hidden_proto(rc_make_env)
librc_hidden_proto(rc_rm_dir)
librc_hidden_proto(rc_runlevel_exists)
librc_hidden_proto(rc_runlevel_get)
librc_hidden_proto(rc_runlevel_list)
librc_hidden_proto(rc_runlevel_set)
librc_hidden_proto(rc_runlevel_starting)
librc_hidden_proto(rc_runlevel_stopping)
librc_hidden_proto(rc_service_add)
librc_hidden_proto(rc_service_daemons_crashed)
librc_hidden_proto(rc_service_daemon_set)
librc_hidden_proto(rc_service_delete)
librc_hidden_proto(rc_service_description)
librc_hidden_proto(rc_service_exists)
librc_hidden_proto(rc_service_in_runlevel)
librc_hidden_proto(rc_service_mark)
librc_hidden_proto(rc_service_option_get)
librc_hidden_proto(rc_service_option_set)
librc_hidden_proto(rc_service_options)
librc_hidden_proto(rc_service_plugable)
librc_hidden_proto(rc_service_resolve)
librc_hidden_proto(rc_service_schedule_clear)
librc_hidden_proto(rc_service_schedule_start)
librc_hidden_proto(rc_service_start)
librc_hidden_proto(rc_service_stop)
librc_hidden_proto(rc_service_wait)
librc_hidden_proto(rc_services_in_runlevel)
librc_hidden_proto(rc_services_in_state)
librc_hidden_proto(rc_services_scheduled)
librc_hidden_proto(rc_services_scheduled_by)
librc_hidden_proto(rc_service_started_daemon)
librc_hidden_proto(rc_service_state)
librc_hidden_proto(rc_strcatpaths)
librc_hidden_proto(rc_strlist_add)
librc_hidden_proto(rc_strlist_addu)
librc_hidden_proto(rc_strlist_addsort)
librc_hidden_proto(rc_strlist_addsortc)
librc_hidden_proto(rc_strlist_addsortu)
librc_hidden_proto(rc_strlist_delete)
librc_hidden_proto(rc_strlist_free)
librc_hidden_proto(rc_strlist_join)
librc_hidden_proto(rc_strlist_reverse)
librc_hidden_proto(rc_waitpid)
librc_hidden_proto(rc_xmalloc)
librc_hidden_proto(rc_xrealloc)
librc_hidden_proto(rc_xstrdup)

#endif
