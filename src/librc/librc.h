/*
 * librc.h
 * Internal header file to setup build env for files in librc.so
 */

/*
 * Copyright (c) 2007-2008 Roy Marples <roy@marples.name>
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

#ifndef _LIBRC_H_
#define _LIBRC_H_

#define _IN_LIBRC

#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/wait.h>

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <limits.h>
#include <paths.h>
#include <regex.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <unistd.h>

#ifdef BSD
#include <sys/param.h>
#include <sys/user.h>
#include <sys/sysctl.h>
#include <kvm.h>
#endif

#include "rc.h"
#include "rc-misc.h"

#include "hidden-visibility.h"
#define librc_hidden_proto(x) hidden_proto(x)
#define librc_hidden_def(x) hidden_def(x)

librc_hidden_proto(rc_conf_value)
librc_hidden_proto(rc_config_list)
librc_hidden_proto(rc_config_load)
librc_hidden_proto(rc_config_value)
librc_hidden_proto(rc_deptree_depend)
librc_hidden_proto(rc_deptree_depends)
librc_hidden_proto(rc_deptree_free)
librc_hidden_proto(rc_deptree_load)
librc_hidden_proto(rc_deptree_load_file)
librc_hidden_proto(rc_deptree_order)
librc_hidden_proto(rc_deptree_update)
librc_hidden_proto(rc_deptree_update_needed)
librc_hidden_proto(rc_find_pids)
librc_hidden_proto(rc_getfile)
librc_hidden_proto(rc_getline)
librc_hidden_proto(rc_newer_than)
librc_hidden_proto(rc_proc_getent)
librc_hidden_proto(rc_older_than)
librc_hidden_proto(rc_runlevel_exists)
librc_hidden_proto(rc_runlevel_get)
librc_hidden_proto(rc_runlevel_list)
librc_hidden_proto(rc_runlevel_set)
librc_hidden_proto(rc_runlevel_stack)
librc_hidden_proto(rc_runlevel_stacks)
librc_hidden_proto(rc_runlevel_starting)
librc_hidden_proto(rc_runlevel_stopping)
librc_hidden_proto(rc_runlevel_unstack)
librc_hidden_proto(rc_service_add)
librc_hidden_proto(rc_service_daemons_crashed)
librc_hidden_proto(rc_service_daemon_set)
librc_hidden_proto(rc_service_delete)
librc_hidden_proto(rc_service_description)
librc_hidden_proto(rc_service_exists)
librc_hidden_proto(rc_service_extra_commands)
librc_hidden_proto(rc_service_in_runlevel)
librc_hidden_proto(rc_service_mark)
librc_hidden_proto(rc_service_resolve)
librc_hidden_proto(rc_service_schedule_clear)
librc_hidden_proto(rc_service_schedule_start)
librc_hidden_proto(rc_services_in_runlevel)
librc_hidden_proto(rc_services_in_runlevel_stacked)
librc_hidden_proto(rc_services_in_state)
librc_hidden_proto(rc_services_scheduled)
librc_hidden_proto(rc_services_scheduled_by)
librc_hidden_proto(rc_service_started_daemon)
librc_hidden_proto(rc_service_state)
librc_hidden_proto(rc_service_value_get)
librc_hidden_proto(rc_service_value_set)
librc_hidden_proto(rc_stringlist_add)
librc_hidden_proto(rc_stringlist_addu)
librc_hidden_proto(rc_stringlist_delete)
librc_hidden_proto(rc_stringlist_find)
librc_hidden_proto(rc_stringlist_free)
librc_hidden_proto(rc_stringlist_new)
librc_hidden_proto(rc_stringlist_split)
librc_hidden_proto(rc_stringlist_sort)
librc_hidden_proto(rc_sys)
librc_hidden_proto(rc_sys_v1)
librc_hidden_proto(rc_sys_v2)
librc_hidden_proto(rc_yesno)

#endif
