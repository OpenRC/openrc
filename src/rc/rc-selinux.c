/*
 * rc-selinux.c
 * SELinux helpers to get and set contexts.
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
#include <ctype.h>
#include <limits.h>
#include <pwd.h>
#include <unistd.h>

#include <selinux/selinux.h>
#include <selinux/label.h>
#include <selinux/get_default_type.h>
#include <selinux/context.h>

#include <sys/stat.h>
#include <sys/types.h>

#include "einfo.h"
#include "queue.h"
#include "rc.h"
#include "rc-misc.h"
#include "rc-plugin.h"
#include "rc-selinux.h"

/* the context files for selinux */
#define RUN_INIT_FILE "run_init_type"
#define INITRC_FILE "initrc_context"

#ifdef HAVE_AUDIT
#include <libaudit.h>
#endif

/* PAM or shadow for authentication */
#ifdef HAVE_PAM
#    define PAM_SERVICE_NAME "run_init" /* the name of this program for PAM */
#    include <security/pam_appl.h>
#    include <security/pam_misc.h>
#else
#    define PASSWORD_PROMPT "Password:"
#    include <crypt.h>
#    include <shadow.h>
#    include <string.h>
#endif


/* The handle for the fcontext lookups */
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

/*
 * This will check the users password and return 0 on success or -1 on fail
 *
 * We ask for the password to make sure it is intended vs run by malicious software.
 * Actual authorization is covered by the policy itself.
 */
static int check_password(char *username)
{
	int ret = 1;
#ifdef HAVE_PAM
	pam_handle_t *pamh;
	int pam_err = 0;
	const struct pam_conv pconv = {
		misc_conv,
		NULL
	};

	pam_err = pam_start(PAM_SERVICE_NAME, username, &pconv, &pamh);
	if (pam_err != PAM_SUCCESS) {
		ret = -1;
		goto outpam;
	}

	pam_err = pam_authenticate(pamh, PAM_DISALLOW_NULL_AUTHTOK);
	if (pam_err != PAM_SUCCESS) {
		ret = -1;
		goto outpam;
	}

	ret = 0;
outpam:
	pam_end(pamh, pam_err);
	pamh = NULL;

#else /* authenticating via /etc/shadow instead */
	struct spwd *spw;
	char *password;
	char *attempt;

	spw = getspnam(username);
	if (!spw) {
		eerror("Failed to read shadow entry");
		ret = -1;
		goto outshadow;
	}

	attempt = getpass(PASSWORD_PROMPT);
	if (!attempt) {
		ret = -1;
		goto outshadow;
	}

	if (*spw->sp_pwdp == '\0' && *attempt == '\0') {
		ret = -1;
		goto outshadow;
	}

	/* salt must be at least two characters long */
	if (!(spw->sp_pwdp[0] && spw->sp_pwdp[1])) {
		ret = -1;
		goto outshadow;
	}

	/* encrypt the password attempt */
	password = crypt(attempt, spw->sp_pwdp);

	if (password && strcmp(password, spw->sp_pwdp) == 0)
		ret = 0;
	else
		ret = -1;
outshadow:
#endif
	return ret;
}

/* Authenticates the user, returns 0 on success, 1 on fail */
static int check_auth()
{
	struct passwd *pw;
	uid_t uid;

#ifdef HAVE_AUDIT
	uid = audit_getloginuid();
	if (uid == (uid_t) -1)
		uid = getuid();
#else
	uid = getuid();
#endif

	pw = getpwuid(uid);
	if (!pw) {
		eerror("cannot find your entry in the passwd file.");
		return (-1);
	}

	printf("Authenticating %s.\n", pw->pw_name);

	/* do the actual check */
	if (check_password(pw->pw_name) == 0) {
		return 0;
	}

	eerrorx("Authentication failed for %s", pw->pw_name);
	return 1;
}

/*
 * Read the context from the given context file. context must be free'd by the user.
 */
static int read_context_file(const char *filename, char **context)
{
	int ret = -1;
	FILE *fp;
	char filepath[PATH_MAX];
	char *line = NULL;
	char *p;
	char *p2;
	size_t len = 0;
	ssize_t read;

	memset(filepath, '\0', PATH_MAX);
	snprintf(filepath, PATH_MAX - 1, "%s/%s", selinux_contexts_path(), filename);

	fp = fopen(filepath, "r");
	if (fp == NULL) {
		eerror("Failed to open context file: %s", filename);
		return -1;
	}

	while ((read = getline(&line, &len, fp)) != -1) {
		/* cut off spaces before the string */
		p = line;
		while (isspace(*p) && *p != '\0')
			p++;

		/* empty string, skip */
		if (*p == '\0')
			continue;

		/* cut off spaces after the string */
		p2 = p;
		while (!isspace(*p2) && *p2 != '\0')
			p2++;
		*p2 = '\0';

		*context = xstrdup(p);
		ret = 0;
		break;
	}

	free(line);
	fclose(fp);
	return ret;
}

void selinux_setup(char **argv)
{
	char *new_context = NULL;
	char *curr_context = NULL;
	context_t curr_con;
	char *curr_t = NULL;
	char *run_init_t = NULL;

	/* Return, if selinux is disabled. */
	if (is_selinux_enabled() < 1) {
		return;
	}

	if (read_context_file(RUN_INIT_FILE, &run_init_t) != 0) {
		/* assume a reasonable default, rather than bailing out */
		run_init_t = xstrdup("run_init_t");
		ewarn("Assuming SELinux run_init type is %s", run_init_t);
	}

	/* Get our current context. */
	if (getcon(&curr_context) < 0) {
		if (errno == ENOENT) {
			/* should only hit this if proc is not mounted.  this
			 * happens on Gentoo right after init starts, when
			 * the init script processing starts.
			 */
			goto out;
		} else {
			perror("getcon");
			exit(1);
		}
	}

	/* extract the type from the context */
	curr_con = context_new(curr_context);
	curr_t = xstrdup(context_type_get(curr_con));
	/* dont need them anymore so free() now */
	context_free(curr_con);
	free(curr_context);

	/* if we are not in the run_init domain, we should not do anything */
	if (strncmp(run_init_t, curr_t, strlen(run_init_t)) != 0) {
		goto out;
	}

	free(curr_t);
	free(run_init_t);

	if (check_auth() != 0) {
		eerrorx("Authentication failed.");
	}

	/* Get the context for the script to be run in. */
	if (read_context_file(INITRC_FILE, &new_context) != 0) {
		/* assume a reasonable default, rather than bailing out */
		new_context = xstrdup("system_u:system_r:initrc_t");
		ewarn("Assuming SELinux initrc context is %s", new_context);
	}

	/* Set the new context */
	if (setexeccon(new_context) < 0) {
		eerrorx("Could not set SELinux exec context to %s.", new_context);
	}

	free(new_context);

	/*
	 * exec will recycle ptys so try and use open_init_pty if it exists
	 * which will open the pty with initrc_devpts_t, if it doesnt exist,
	 * fall back to plain exec
	 */
	if (access("/usr/sbin/open_init_pty", X_OK)) {
		if (execvp("/usr/sbin/open_init_pty", argv)) {
			perror("execvp");
			exit(-1);
		}
	} else if (execvp(argv[1], argv + 1)) {
		perror("execvp");
		exit(-1);
	}

out:
	free(run_init_t);
	free(curr_t);
}
