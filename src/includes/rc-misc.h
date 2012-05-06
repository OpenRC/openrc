/*
  rc-misc.h
  This is private to us and not for user consumption
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

#ifndef __RC_MISC_H__
#define __RC_MISC_H__

#include <sys/stat.h>
#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "helpers.h"

#define RC_LEVEL_BOOT           "boot"
#define RC_LEVEL_DEFAULT        "default"

#define RC_DEPTREE_CACHE        RC_SVCDIR "/deptree"
#define RC_DEPTREE_SKEWED	RC_SVCDIR "/clock-skewed"
#define RC_KRUNLEVEL            RC_SVCDIR "/krunlevel"
#define RC_STARTING             RC_SVCDIR "/rc.starting"
#define RC_STOPPING             RC_SVCDIR "/rc.stopping"

#define RC_SVCDIR_STARTING      RC_SVCDIR "/starting"
#define RC_SVCDIR_INACTIVE      RC_SVCDIR "/inactive"
#define RC_SVCDIR_STARTED       RC_SVCDIR "/started"
#define RC_SVCDIR_COLDPLUGGED	RC_SVCDIR "/coldplugged"

_unused static bool exists(const char *pathname)
{
	struct stat buf;

	return (stat(pathname, &buf) == 0);
}

_unused static bool existss(const char *pathname)
{
	struct stat buf;

	return (stat(pathname, &buf) == 0 && buf.st_size != 0);
}

char *rc_conf_value(const char *var);
bool rc_conf_yesno(const char *var);
void env_filter(void);
void env_config(void);
int signal_setup(int sig, void (*handler)(int));
int svc_lock(const char *);
int svc_unlock(const char *, int);
pid_t exec_service(const char *, const char *);

/*
 * Check whether path is writable or not,
 * this also works properly with read-only filesystems
 */
int is_writable(const char *);

#define service_start(service) exec_service(service, "start");
#define service_stop(service)  exec_service(service, "stop");

int parse_mode(mode_t *, char *);
#endif
