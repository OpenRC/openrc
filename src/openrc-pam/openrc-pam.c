#include <pwd.h>
#include <grp.h>
#include <security/pam_modules.h>
#include <security/pam_appl.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/file.h>
#include <syslog.h>
#include <unistd.h>

#include "librc.h"
#include "einfo.h"

static int
exec_openrc(pam_handle_t *pamh, bool opening)
{
	char *svc_name = NULL;
	char *pam_lock = NULL;
	char *logins, *rundir;
	char *file;
	const char *username;
	struct passwd *pw;
	int count = 0, fd = -1;
	int pid = -1, status;
	int ret = PAM_SUCCESS;
	RC_SERVICE service_status;

	if (pam_get_user(pamh, &username, "username:") != PAM_SUCCESS)
		return PAM_SESSION_ERR;

	pw = getpwnam(username);
	if (!pw)
		return PAM_SESSION_ERR;

	if (pw->pw_uid == 0)
		return PAM_SUCCESS;

	xasprintf(&pam_lock, "openrc-pam.%s", pw->pw_name);
	setenv("EINFO_LOG", pam_lock, true);

	xasprintf(&svc_name, "user.%s", pw->pw_name);
	service_status = rc_service_state(svc_name);
	if (service_status & RC_SERVICE_STARTED && !(service_status & RC_SERVICE_HOTPLUGGED)) {
		elog(LOG_INFO, "service started and not hotplugged, skipping session.");
		goto out;
	}

	elog(LOG_INFO, opening ? "starting session" : "stopping session");
	fd = svc_lock(pam_lock, false);

	if (fd == -1) {
		ret = PAM_SESSION_ERR;
		goto out;
	}

	file = rc_service_resolve(svc_name);
	if (!file) {
		file = rc_service_dylink("user", svc_name);
		if (!file) {
			svc_unlock(pam_lock, fd);
			goto out;
		}
	}

	logins = rc_service_value_get(svc_name, "logins");
	if (logins)
		sscanf(logins, "%d", &count);
	free(logins);

	if (opening) {
		if (count == 0) {
			pid = service_start(file);
			rc_service_mark(svc_name, RC_SERVICE_HOTPLUGGED);
		}
		count++;
	} else {
		count--;
		if (count == 0)
			pid = service_stop(file);
	}

	elog(LOG_INFO, "%d sessions", count);

	if (pid > 0) {
		waitpid(pid, &status, 0);
		if (status != 0)
			ret = PAM_SESSION_ERR;
	}

	xasprintf(&logins, "%d", count);
	rc_service_value_set(svc_name, "logins", logins);
	free(logins);

	rundir = rc_service_value_get(svc_name, "xdg_runtime_dir");
	if (rundir) {
		char *rundir_env;
		xasprintf(&rundir_env, "XDG_RUNTIME_DIR=%s", rundir);
		pam_putenv(pamh, rundir_env);
		free(rundir_env);
		free(rundir);
	}

	svc_unlock(pam_lock, fd);

out:
	free(pam_lock);
	free(svc_name);
	unsetenv("EINFO_LOG");
	return ret;
}

PAM_EXTERN int pam_sm_open_session(pam_handle_t *pamh, int flags, int argc, const char **argv) {
	(void) argc;
	(void) argv;
	(void) flags;

	return exec_openrc(pamh, true);
}

PAM_EXTERN int pam_sm_close_session(pam_handle_t *pamh, int flags, int argc, const char **argv) {
	(void) argc;
	(void) argv;
	(void) flags;

	return exec_openrc(pamh, false);
}
