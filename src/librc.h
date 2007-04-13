/*
 * librc.h
 * Internal header file to setup build env for files in librc.so
 * Copyright 2007 Gentoo Foundation
 * Released under the GPLv2
 */

#ifndef _LIBRC_H_
#define _LIBRC_H_

#include <sys/select.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <sys/wait.h>

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
#include <unistd.h>

#if defined(__DragonFly__) || defined(__FreeBSD__) || \
	defined(__NetBSD__) || defined (__OpenBSD__)
#include <sys/param.h>
#include <sys/user.h>
#include <sys/sysctl.h>
#include <kvm.h>
#endif

#include "einfo.h"
#include "rc.h"
#include "rc-misc.h"
#include "strlist.h"

/* internal alias trickery! we dont want internal relocs! */
#if defined(__ELF__) && defined(__GNUC__)
# define __hidden_asmname(name) __hidden_asmname1 (__USER_LABEL_PREFIX__, name)
# define __hidden_asmname1(prefix, name) __hidden_asmname2(prefix, name)
# define __hidden_asmname2(prefix, name) #prefix name
# define __hidden_proto(name, internal) \
	extern __typeof (name) name __asm__ (__hidden_asmname (#internal)) \
	__attribute__ ((visibility ("hidden")));
# define __hidden_ver(local, internal, name) \
   extern __typeof (name) __EI_##name __asm__(__hidden_asmname (#internal)); \
   extern __typeof (name) __EI_##name __attribute__((alias (__hidden_asmname1 (,#local))))
# define librc_hidden_proto(name) __hidden_proto(name, __RC_##name)
# define librc_hidden_def(name) __hidden_ver(__RC_##name, name, name);
#else
# define librc_hidden_proto(name)
# define librc_hidden_def(name)
#endif

librc_hidden_proto(rc_allow_plug)
librc_hidden_proto(rc_config_env)
librc_hidden_proto(rc_exists)
librc_hidden_proto(rc_filter_env)
librc_hidden_proto(rc_find_pids)
librc_hidden_proto(rc_free_deptree)
librc_hidden_proto(rc_get_config)
librc_hidden_proto(rc_get_config_entry)
librc_hidden_proto(rc_get_depends)
librc_hidden_proto(rc_get_depinfo)
librc_hidden_proto(rc_get_deptype)
librc_hidden_proto(rc_get_list)
librc_hidden_proto(rc_get_runlevel)
librc_hidden_proto(rc_get_runlevels)
librc_hidden_proto(rc_get_service_option)
librc_hidden_proto(rc_is_dir)
librc_hidden_proto(rc_is_env)
librc_hidden_proto(rc_is_exec)
librc_hidden_proto(rc_is_file)
librc_hidden_proto(rc_is_link)
librc_hidden_proto(rc_load_deptree)
librc_hidden_proto(rc_ls_dir)
librc_hidden_proto(rc_mark_service)
librc_hidden_proto(rc_order_services)
librc_hidden_proto(rc_resolve_service)
librc_hidden_proto(rc_rm_dir)
librc_hidden_proto(rc_runlevel_exists)
librc_hidden_proto(rc_runlevel_starting)
librc_hidden_proto(rc_runlevel_stopping)
librc_hidden_proto(rc_schedule_clear)
librc_hidden_proto(rc_schedule_start_service)
librc_hidden_proto(rc_service_add)
librc_hidden_proto(rc_service_daemons_crashed)
librc_hidden_proto(rc_service_delete)
librc_hidden_proto(rc_service_exists)
librc_hidden_proto(rc_service_in_runlevel)
librc_hidden_proto(rc_services_in_runlevel)
librc_hidden_proto(rc_services_in_state)
librc_hidden_proto(rc_services_scheduled)
librc_hidden_proto(rc_services_scheduled_by)
librc_hidden_proto(rc_service_started_daemon)
librc_hidden_proto(rc_service_state)
librc_hidden_proto(rc_set_runlevel)
librc_hidden_proto(rc_set_service_daemon)
librc_hidden_proto(rc_set_service_option)
librc_hidden_proto(rc_start_service)
librc_hidden_proto(rc_stop_service)
librc_hidden_proto(rc_strcatpaths)
librc_hidden_proto(rc_strlist_add)
librc_hidden_proto(rc_strlist_addsort)
librc_hidden_proto(rc_strlist_addsortc)
librc_hidden_proto(rc_strlist_addsortu)
librc_hidden_proto(rc_strlist_delete)
librc_hidden_proto(rc_strlist_free)
librc_hidden_proto(rc_strlist_reverse)
librc_hidden_proto(rc_update_deptree)
librc_hidden_proto(rc_wait_service)
librc_hidden_proto(rc_xcalloc)
librc_hidden_proto(rc_xmalloc)
librc_hidden_proto(rc_xrealloc)
librc_hidden_proto(rc_xstrdup)

#endif
