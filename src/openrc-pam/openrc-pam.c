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
	char *multiplex = NULL;
	char *logins, *rundir;
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

	xasprintf(&svc_name, "user.%s", pw->pw_name);

	service_status = rc_service_state(svc_name);
	if (service_status & RC_SERVICE_STARTED && !(service_status & RC_SERVICE_HOTPLUGGED))
		goto out;

	xasprintf(&pam_lock, "openrc-pam.%s", pw->pw_name);
	fd = svc_lock(pam_lock, false);

	if (fd == -1) {
		ret = PAM_SESSION_ERR;
		goto out;
	}

	if (!rc_service_exists(svc_name)) {
		multiplex = rc_service_multiplex("user", pw->pw_name);
		if (!multiplex) {
			ret = PAM_SESSION_ERR;
			goto unlock;
		}
	}

	logins = rc_service_value_get(svc_name, "logins");
	if (logins)
		sscanf(logins, "%d", &count);
	free(logins);

	if (opening && count++ == 0) {
		pid = service_start(svc_name);
		rc_service_mark(svc_name, RC_SERVICE_HOTPLUGGED);
	} else if (--count == 0) {
		pid = service_stop(svc_name);
	}

	if (pid > 0) {
		waitpid(pid, &status, 0);
		if (status != 0)
			ret = PAM_SESSION_ERR;
	}

	xasprintf(&logins, "%d", count);
	rc_service_value_set(svc_name, "logins", logins);
	free(logins);

unlock:
	svc_unlock(pam_lock, fd);

out:
	rundir = rc_service_value_get(svc_name, "xdg_runtime_dir");
	if (rundir) {
		char *rundir_env;
		xasprintf(&rundir_env, "XDG_RUNTIME_DIR=%s", rundir);
		pam_putenv(pamh, rundir_env);
		free(rundir_env);
		free(rundir);
	}

	free(pam_lock);
	free(svc_name);
	free(multiplex);
	return ret;
}

PAM_EXTERN int pam_sm_open_session(pam_handle_t *pamh, int flags, int argc, const char **argv) {
	(void) argc; (void) argv; (void) flags;

	return exec_openrc(pamh, true);
}

PAM_EXTERN int pam_sm_close_session(pam_handle_t *pamh, int flags, int argc, const char **argv) {
	(void) argc; (void) argv; (void) flags;

	return exec_openrc(pamh, false);
}