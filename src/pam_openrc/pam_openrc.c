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
	char *svc_name, *pam_lock, *logins, *script = NULL;
	const char *username = NULL, *session = NULL;
	RC_SERVICE service_status;
	int count = 0, fd, status;
	int ret = PAM_SUCCESS;
	struct passwd *user;
	pid_t pid = -1;

	setenv("EINFO_LOG", "pam_openrc", true);

	if (pam_get_item(pamh, PAM_SERVICE, (const void **)&session) != PAM_SUCCESS) {
		elog(LOG_ERR, "Failed to get PAM_SERVICE");
		return PAM_SESSION_ERR;
	}

	if (session && strcmp(session, "openrc-user") == 0)
		return PAM_SUCCESS;

	if (pam_get_item(pamh, PAM_USER, (const void **)&username) != PAM_SUCCESS)
		return PAM_SESSION_ERR;

	if (!username) {
		elog(LOG_ERR, "PAM_USER unset.");
		return PAM_SESSION_ERR;
	}

	if (!(user = getpwnam(username))) {
		elog(LOG_ERR, "User '%s' not found.", username);
		return PAM_SESSION_ERR;
	}

	if (user->pw_uid == 0)
		return PAM_SUCCESS;

	xasprintf(&pam_lock, "pam_openrc[%s]", user->pw_name);
	setenv("EINFO_LOG", pam_lock, true);

	xasprintf(&svc_name, "user.%s", user->pw_name);

	service_status = rc_service_state(svc_name);
	if (service_status & RC_SERVICE_STARTED && !(service_status & RC_SERVICE_HOTPLUGGED)) {
		elog(LOG_INFO, "%s started and not hotplugged, skipping session.", svc_name);
		goto out;
	}

	elog(LOG_INFO, opening ? "starting session" : "stopping session");
	fd = svc_lock(pam_lock, false);

	if (fd == -1) {
		ret = PAM_SESSION_ERR;
		goto out;
	}

	if (!(script = rc_service_resolve(svc_name))) {
		if (!(script = rc_service_resolve("user"))) {
			elog(LOG_ERR, "Failed to resolve %s.", svc_name);
			ret = PAM_SESSION_ERR;
			goto out;
		}

		if (symlinkat(script, rc_dirfd(RC_DIR_INITD), svc_name) != 0) {
			elog(LOG_ERR, "symlink: %s", strerror(errno));
			ret = PAM_SESSION_ERR;
			goto out;
		}
	}

	logins = rc_service_value_get(svc_name, "logins");
	if (logins)
		sscanf(logins, "%d", &count);
	free(logins);

	if (opening) {
		if (count == 0) {
			pid = service_start(svc_name);
			rc_service_mark(svc_name, RC_SERVICE_HOTPLUGGED);
		}
		count++;
	} else {
		count--;
		if (count == 0)
			pid = service_stop(svc_name);
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

out:
	svc_unlock(pam_lock, fd);
	free(pam_lock);
	free(svc_name);
	free(script);
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
