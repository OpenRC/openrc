/*
   librc-plugin.h 
   Private instructions to use plugins
   Copyright 2007 Gentoo Foundation
   Released under the GPLv2
   */

#ifndef __LIBRC_PLUGIN_H__
#define __LIBRC_PLUGIN_H__

void rc_plugin_load ();
void rc_plugin_unload ();
void rc_plugin_run (rc_hook_t, const char *value);

/* dlfunc defines needed to avoid ISO errors. FreeBSD has this right :) */
#ifndef __FreeBSD__
struct __dlfunc_arg {
	int	__dlfunc_dummy;
};

typedef	void (*dlfunc_t) (struct __dlfunc_arg);

dlfunc_t dlfunc (void * __restrict handle, const char * __restrict symbol);
#endif

#endif
