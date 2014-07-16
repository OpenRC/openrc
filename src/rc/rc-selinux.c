/*
  rc-selinux.c
  SELinux helpers to get and set contexts.
*/

/*
 * Copyright (c) 2014 Jason Zaman <jason@perfinion.com>
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

#include <stddef.h>
#include <errno.h>
#include <dlfcn.h>

#include <sys/stat.h>

#include <selinux/selinux.h>
#include <selinux/label.h>

#include "einfo.h"
#include "rc.h"
#include "rc-misc.h"
#include "rc-plugin.h"
#include "rc-selinux.h"

#define SELINUX_LIB     RC_LIBDIR "/runscript_selinux.so"

static void (*selinux_run_init_old) (void);
static void (*selinux_run_init_new) (int argc, char **argv);

static struct selabel_handle *hnd = NULL;

int selinux_util_label(const char *path)
{
	int retval = 0;
	int enforce;
	struct stat st;
	security_context_t con;

	enforce = security_getenforce();
	if (retval < 0)
		return retval;

	if (!hnd)
		return (enforce) ? -1 : 0;

	retval = lstat(path, &st);
	if (retval < 0) {
		if (errno == ENOENT)
			return 0;
		return (enforce) ? -1 : 0;
	}

	/* lookup the context */
	retval = selabel_lookup_raw(hnd, &con, path, st.st_mode);
	if (retval < 0) {
		if (errno == ENOENT)
			return 0;
		return (enforce) ? -1 : 0;
	}

	/* apply the context */
	retval = lsetfilecon(path, con);
	freecon(con);
	if (retval < 0) {
		if (errno == ENOENT)
			return 0;
		if (errno == ENOTSUP)
			return 0;
		return (enforce) ? -1 : 0;
	}

	return 0;
}

/*
 * Open the label handle
 * returns 1 on success, 0 if no selinux, negative on error
 */
int selinux_util_open(void)
{
	int retval = 0;

	retval = is_selinux_enabled();
	if (retval <= 0)
		return retval;

	hnd = selabel_open(SELABEL_CTX_FILE, NULL, 0);
	if (!hnd)
		return -2;

	return 1;
}

/*
 * Close the label handle
 * returns 1 on success, 0 if no selinux, negative on error
 */
int selinux_util_close(void)
{
	int retval = 0;

	retval = is_selinux_enabled();
	if (retval <= 0)
		return retval;

	if (hnd) {
		selabel_close(hnd);
		hnd = NULL;
	}

	return 0;
}

void selinux_setup(int argc, char **argv)
{
	void *lib_handle = NULL;

	if (!exists(SELINUX_LIB))
		return;

	lib_handle = dlopen(SELINUX_LIB, RTLD_NOW | RTLD_GLOBAL);
	if (!lib_handle) {
		eerror("dlopen: %s", dlerror());
		return;
	}

	selinux_run_init_old = (void (*)(void))
	    dlfunc(lib_handle, "selinux_runscript");
	selinux_run_init_new = (void (*)(int, char **))
	    dlfunc(lib_handle, "selinux_runscript2");

	/* Use new run_init if it exists, else fall back to old */
	if (selinux_run_init_new)
		selinux_run_init_new(argc, argv);
	else if (selinux_run_init_old)
		selinux_run_init_old();
	else
		/* This shouldnt happen... probably corrupt lib */
		eerrorx
		    ("run_init is missing from runscript_selinux.so!");

	dlclose(lib_handle);
}
