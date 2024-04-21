#include <librc.h>
#include <pwd.h>
#include <grp.h>
#include <security/pam_modules.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/file.h>
#include <syslog.h>
#include <unistd.h>

#include "einfo.h"
#include "queue.h"

static inline bool
check_rundir(struct stat *sb, struct passwd *pw)
{
	return sb->st_uid == pw->pw_uid && sb->st_gid == pw->pw_gid;
}

static size_t
get_session_count(const char *user)
{
	char *count_str = rc_user_value_get(user, "session_count");
	size_t count;

	if (!count_str || sscanf(count_str, "%lu", &count) == 0)
		count = 0;
	free(count_str);
	return count;
}

static bool
set_session_count(const char *user, size_t count)
{
	char *value;
	bool ret;

	if (count == 0)
		return rc_user_value_set(user, "session_count", NULL);

	xasprintf(&value, "%ld", count);
	ret = rc_user_value_set(user, "session_count", value);
	free(value);
	return ret;
}

static int
exec_user_cmd(pam_handle_t *pamh, struct passwd *pw, char *cmd)
{
	int retval;
	const char *shellname = basename_c(pw->pw_shell);
	char **envron;

	elog(LOG_INFO, "Executing %s", cmd);

	switch (fork()) {
		case 0:
			initgroups(pw->pw_name, pw->pw_gid);
			setgid(pw->pw_gid);
			setuid(pw->pw_uid);

			envron = pam_getenvlist(pamh);
			execle(pw->pw_shell, shellname, "-c", cmd, NULL, envron);

			elog(LOG_ERR, "failed to exec \"%s, %s, -c, %s\": %s",
					pw->pw_shell, shellname, cmd, strerror(errno));
			return -1;
			break;
		case -1:
			return -1;
			break;
	}
	wait(&retval);
	return retval;
}

static void
export_rundir(pam_handle_t *pamh, const char *rundir)
{
	char *env;
	xasprintf(&env, "XDG_RUNTIME_DIR=%s", rundir);
	pam_putenv(pamh, env);
	free(env);
}

static char *
ensure_xdg_rundir(pam_handle_t *pamh, struct passwd *pw)
{
	const char *env = pam_getenv(pamh, "XDG_RUNTIME_DIR");
	char *rundir;
	struct stat sb;

	if (env) {
		if (stat(env, &sb) != 0 || !check_rundir(&sb, pw)) {
			elog(LOG_ERR, "%s does not belong to uid %d", rundir, pw->pw_uid);
			return NULL;
		}
		return xstrdup(env);
	}

	xasprintf(&rundir, "/run/user/%d", pw->pw_uid);
	if (stat(rundir, &sb) == 0) {
		if (!check_rundir(&sb, pw)) {
			elog(LOG_ERR, "%s does not belong to uid %d", rundir, pw->pw_uid);
			free(rundir);
			return NULL;
		}
		export_rundir(pamh, rundir);
		return rundir;
	}

	elog(LOG_INFO, "Creating runtime directory %s for uid %d", rundir, pw->pw_uid);

	if ((mkdir(rundir, 0700) != 0 && errno != EEXIST)
			|| chown(rundir, pw->pw_uid, pw->pw_gid) != 0) {
		elog(LOG_ERR, "Failed create runtime directory %s: %s",
			rundir, strerror(errno));
		free(rundir);
		return NULL;
	}

	rc_user_value_set(pw->pw_name, "rundir_managed", "yes");

	export_rundir(pamh, rundir);
	return rundir;
}

static DIR *
try_lock_dir(const char *dir)
{
	DIR *lock;

	lock = opendir(dir);
	if (!lock)
		return NULL;

	for (size_t tries = 0; tries < 3; tries++) {
		if (flock(dirfd(lock), LOCK_EX | LOCK_NB) == 0) {
			return lock;
		} else if (errno != EWOULDBLOCK) {
			closedir(lock);
			return NULL;
		}
		elog(LOG_WARNING, "Failed to lock %s, trying %lu more times.", dir, 3 - tries);
		sleep(1);
	}

	elog(LOG_ERR, "Failed to lock %s.", dir);
	return NULL;
}

static bool
exec_openrc(pam_handle_t *pamh, struct passwd *pw, const char *runlevel, bool going_down)
{
	char *cmd = NULL;
	char *lock_path;
	char *rundir;
	char *static_usr;
	DIR *lock;
	bool ret = true;
	size_t count;

	/* Check if the user session is running statically */
	static_usr = rc_user_value_get(pw->pw_name, "static");
	if (rc_yesno(static_usr)) {
		free(static_usr);
		return true;
	}
	free(static_usr);

	/* Open sysconf dir for the user, and flock it */
	xasprintf(&lock_path, "%s/users/%s", rc_service_dir(), pw->pw_name);
	if (mkdir(lock_path, 0755) != 0 && errno != EEXIST) {
		elog(LOG_ERR, "mkdir '%s': %s", lock_path, strerror(errno));
		free(lock_path);
		return false;
	}

	lock = try_lock_dir(lock_path);
	free(lock_path);
	if (!lock)
		return false;

	rundir = ensure_xdg_rundir(pamh, pw);
	if (!rundir) {
		elog(LOG_ERR, "Failed to create runtime directory");
		ret = false;
		goto out;
	}

	count = get_session_count(pw->pw_name);

	/* If going down, remove the current session from the count */
	if (going_down)
		count--;

	elog(LOG_INFO, "Session count: %lu", count);
	/*
	 * execute the command if the user doesn't have any open sessions
	 */
	if (count == 0) {
		char *managed = rc_user_value_get(pw->pw_name, "rundir_managed");

		xasprintf(&cmd, "openrc --user %s", runlevel);
		if (exec_user_cmd(pamh, pw, cmd) == -1)
			ret = false;

		if (going_down && rc_yesno(managed))
			rm_dir(rundir, true);

		free(managed);
	}

	/* If not going down, add ourselves to the count */
	if (!going_down)
		count++;

	set_session_count(pw->pw_name, count);

out:
	free(cmd);
	free(rundir);
	closedir(lock);
	return ret;
}

static struct passwd *
get_pw(pam_handle_t *pamh)
{
	const char *username;
	if (pam_get_user(pamh, &username, "username:") != PAM_SUCCESS)
		return NULL;
	return getpwnam(username);
}

PAM_EXTERN int pam_sm_open_session(pam_handle_t *pamh, int flags, int argc, const char **argv) {
	const char *runlevel = argc > 0 ? runlevel = argv[0] : "default";
	char *log_name;
	struct passwd *pw;
	int ret = PAM_SUCCESS;
	(void) flags;

	pw = get_pw(pamh);
	if (!pw)
		return PAM_SESSION_ERR;

	if (pw->pw_uid == 0)
		return PAM_SUCCESS;

	xasprintf(&log_name, "openrc-pam[%s]", pw->pw_name);
	setenv("EINFO_LOG", log_name, 1);
	free(log_name);

	elog(LOG_INFO, "Opening openrc session");

	if (!exec_openrc(pamh, pw, runlevel, false)) {
		elog(LOG_ERR, "Failed to close session");
		ret = PAM_SESSION_ERR;
	}

	unsetenv("EINFO_LOG");
	return ret;
}

PAM_EXTERN int pam_sm_close_session(pam_handle_t *pamh, int flags, int argc, const char **argv) {
	const char *runlevel = argc > 1 ? argv[1] : "none";
	char *log_name;
	struct passwd *pw;
	int ret = PAM_SUCCESS;
	(void) flags;

	pw = get_pw(pamh);
	if (!pw)
		return PAM_SESSION_ERR;

	if (pw->pw_uid == 0)
		return PAM_SUCCESS;

	xasprintf(&log_name, "openrc-pam [%s]", pw->pw_name);
	setenv("EINFO_LOG", log_name, 1);
	free(log_name);

	elog(LOG_INFO, "Closing openrc session");

	if (!exec_openrc(pamh, pw, runlevel, true)) {
		elog(LOG_ERR, "Failed to close session");
		ret = PAM_SESSION_ERR;
	}

	unsetenv("EINFO_LOG");
	return ret;
}
