#include <librc.h>
#include <pwd.h>
#include <security/pam_appl.h>
#include <security/pam_ext.h>
#include <security/pam_modules.h>
#include <stdbool.h>
#include <stdio.h>
#include <syslog.h>
#include <sys/file.h>
#include <unistd.h>
#include <string.h>

#include "einfo.h"
#include "queue.h"

static int
inc_dec_lockfile(pam_handle_t *pamh, int val)
{
	char *lockfile_path = NULL;
	FILE *lockfile = NULL;

	int locknum = 0;

	pam_syslog(pamh, LOG_INFO, "locking lockfile");

	xasprintf(&lockfile_path, "%s/openrc/%s", pam_getenv(pamh, "XDG_RUNTIME_DIR"), "lock");
	lockfile = fopen(lockfile_path, "r+");
	if (!lockfile) {
		lockfile = fopen(lockfile_path, "w+");
		if (!lockfile)
			eerrorx("fopen: failed to open file %s, %s", lockfile_path, strerror(errno));
		if (flock(fileno(lockfile), LOCK_EX) != 0) {
			eerrorx("flock: %s", strerror(errno));
		}
		locknum = 1;
	} else {
		if (flock(fileno(lockfile), LOCK_EX) != 0) {
			eerrorx("flock: %s", strerror(errno));
		}
		fscanf(lockfile, "%d", &locknum);
		locknum += val;
		rewind(lockfile);
	}

	free(lockfile_path);

	fprintf(lockfile, "%d", locknum);

	if (flock(fileno(lockfile), LOCK_UN)) {
		eerrorx("flock: %s", strerror(errno));
	}
	fclose(lockfile);

	pam_syslog(pamh, LOG_INFO, "unlocking lockfile");

	return locknum;
}

static void load_envs_from_file(const char *path, RC_STRINGLIST *out) {
	FILE *fp = NULL;
	char *line = NULL;
	size_t n = 0;
	char *p = NULL;

	fp = fopen(path, "r");
	if (!fp) {
		return;
	}
	while (getline(&line, &n, fp) != -1) {
		if ((p = strchr(line, '\n'))) {
			*p = '\0';
		};
		rc_stringlist_addu(out, line);
	}
	fclose(fp);
}

static RC_STRINGLIST *load_dir(const char *dir) {
	RC_STRINGLIST *list = rc_stringlist_new();
	DIR *dp = NULL;
	struct dirent *d = NULL;
	char *path;

	if ((dp = opendir(dir)) != NULL) {
		while ((d = readdir(dp)) != NULL) {
			xasprintf(&path, "%s/%s", dir, d->d_name);
			load_envs_from_file(path, list);
		}
	}
	closedir(dp);

	return list;
}

static void set_user_env(pam_handle_t *pamh) {
	RC_STRINGLIST *allowed_env = NULL;
	RC_STRINGLIST *user_env = NULL;
	RC_STRING *env;
	RC_STRING *uenv;
	char *p;
	char *user_env_path;

	pam_syslog(pamh, LOG_INFO, "Loading allowed envs in %s", RC_USER_ENV_WHITELIST_D);
	allowed_env = load_dir(RC_USER_ENV_WHITELIST_D);

	pam_syslog(pamh, LOG_INFO, "Loading allowed envs in %s", RC_USER_ENV_WHITELIST);
	load_envs_from_file(RC_USER_ENV_WHITELIST, allowed_env);

	xasprintf(&user_env_path, "%s/openrc/env", pam_getenv(pamh, "XDG_RUNTIME_DIR"));

	pam_syslog(pamh, LOG_INFO, "Loading user envs in %s", user_env_path);
	user_env = load_dir(user_env_path);

	TAILQ_FOREACH(env, allowed_env, entries) {
		pam_syslog(pamh, LOG_INFO, "allowed env %s", env->value);
		TAILQ_FOREACH(uenv, user_env, entries) {
			p = strchr(uenv->value, '=');
			if (p) {
				*p = '\0';
				if (strcmp(env->value, uenv->value) == 0) {
					*p = '=';
					pam_syslog(pamh, LOG_INFO, "Exporting: %s", uenv->value);
					pam_putenv(pamh, uenv->value);
				} else {
					*p = '=';
				}
			}
		}
	}

	pam_syslog(pamh, LOG_INFO, "Finished loading user environment");

	rc_stringlist_free(allowed_env);
	rc_stringlist_free(user_env);
	free(user_env_path);
}

static int
exec_user_cmd(struct passwd *pw, char *cmd, char **envlist)
{
	int retval;
	const char *shellname = basename_c(pw->pw_shell);

	switch (fork()) {
		case 0:
			setgid(pw->pw_gid);
			setuid(pw->pw_uid);

			execle(pw->pw_shell, shellname, "-c", cmd, NULL, envlist);

			return -1;
			break;
		case -1:
			return -1;
			break;
	}
	wait(&retval);
	return retval;
}

static char *create_xdg_runtime_dir(struct passwd *pw) {
	char *path = NULL;
	char *openrc_path = NULL;

	if (mkdir("/run/user", 0755) != 0 && errno != EEXIST)
		return NULL;

	xasprintf(&path, "/run/user/%d/", pw->pw_uid);

	if (mkdir(path, 0700) != 0 && errno != EEXIST) {
		free(path);
		return NULL;
	}

	if (chown(path, pw->pw_uid, pw->pw_gid) != 0) {
		free(path);
		return NULL;
	}

	xasprintf(&openrc_path, "%s/%s", path, "openrc");

	if (mkdir(openrc_path, 0700) != 0 && errno != EEXIST) {
		free(openrc_path);
		free(path);
		return NULL;
	}

	if (chown(openrc_path, pw->pw_uid, pw->pw_gid) != 0) {
		free(openrc_path);
		free(path);
		return NULL;
	}

	return path;
}

static bool exec_openrc(pam_handle_t *pamh, const char *runlevel, bool lock) {
	int lockval;
	char *cmd = NULL;
	const char *username;
	struct passwd *pw = NULL;
	char *xdg_runtime_dir;
	char *xdg_runtime_dir_env;
	char **envlist;
	char **env;

	if (pam_get_user(pamh, &username, "username:") != PAM_SUCCESS)
		return false;
	pw = getpwnam(username);
	if (!pw)
		return false;

	/* XDG_RUNTIME_DIR is needed in so many places, that if it's not defined
	 * we're defining it on the standard location. */
	if (pam_getenv(pamh, "XDG_RUNTIME_DIR") == NULL) {
		xdg_runtime_dir = create_xdg_runtime_dir(pw);
		if (!xdg_runtime_dir) {
			return false;
		}

		xasprintf(&xdg_runtime_dir_env, "XDG_RUNTIME_DIR=%s", xdg_runtime_dir);
		pam_putenv(pamh, xdg_runtime_dir_env);
		pam_syslog(pamh, LOG_INFO, "exporting: %s", xdg_runtime_dir_env);
		free(xdg_runtime_dir);
		free(xdg_runtime_dir_env);
	}

	envlist = pam_getenvlist(pamh);

	xasprintf(&cmd, "openrc --user %s", runlevel);

	/* if we are locking, reduce the count by 1,
	 * because we don't want to count ourselves */
	lockval = inc_dec_lockfile(pamh, lock ? 1 : -1) - lock == true ? 1 : 0;

	if (lockval == 0) {
		pam_syslog(pamh, LOG_INFO, "Executing %s for user %s", cmd, username);
		exec_user_cmd(pw, cmd, envlist);
	}

	if (lock) {
		pam_syslog(pamh, LOG_INFO, "Setting the user's environment");
		set_user_env(pamh);
	}

	for (env = envlist; *env; env++)
		free(*env);
	free(envlist);
	free(cmd);
	return true;
}

PAM_EXTERN int pam_sm_open_session(pam_handle_t *pamh, int flags, int argc, const char **argv) {
	const char *runlevel = argc > 0 ? runlevel = argv[0] : "default";
	(void)flags;

	pam_syslog(pamh, LOG_INFO, "Opening openrc session");

	if (exec_openrc(pamh, runlevel, true)) {
		pam_syslog(pamh, LOG_INFO, "Openrc session opened");
		return PAM_SUCCESS;
	} else {
		pam_syslog(pamh, LOG_ERR, "Failed to open session");
		return PAM_SESSION_ERR;
	}
}

PAM_EXTERN int pam_sm_close_session(pam_handle_t *pamh, int flags, int argc, const char **argv) {
	const char *runlevel = argc > 1 ? argv[1] : "none";
	(void)flags;

	pam_syslog(pamh, LOG_INFO, "Closing openrc session");

	if (exec_openrc(pamh, runlevel, false)) {
		pam_syslog(pamh, LOG_INFO, "Openrc session closed");
		return PAM_SUCCESS;
	} else {
		pam_syslog(pamh, LOG_ERR, "Failed to close session");
		return PAM_SESSION_ERR;
	}
}
