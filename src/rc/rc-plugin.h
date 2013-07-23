/*
   librc-plugin.h
   Private instructions to use plugins
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

#ifndef __LIBRC_PLUGIN_H__
#define __LIBRC_PLUGIN_H__

/* A simple flag to say if we're in a plugin proccess or not.
 * Mainly used in atexit code. */
extern bool rc_in_plugin;

int rc_waitpid(pid_t pid);
void rc_plugin_load(void);
void rc_plugin_unload(void);
void rc_plugin_run(RC_HOOK, const char *value);

/* dlfunc defines needed to avoid ISO errors. FreeBSD has this right :) */
#if !defined(__FreeBSD__) && !defined(__DragonFly__)
struct __dlfunc_arg {
	int	__dlfunc_dummy;
};

typedef	void (*dlfunc_t)(struct __dlfunc_arg);

dlfunc_t dlfunc (void * __restrict handle, const char * __restrict symbol);
#endif

#endif
