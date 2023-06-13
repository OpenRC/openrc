#include <stdio.h>
#include <pwd.h>
#include <security/pam_modules.h>
#include <unistd.h>
#include <librc.h>
#include <stdbool.h>
#include <syslog.h>
#ifdef __FreeBSD__
#include <security/pam_appl.h>
#endif

#include "einfo.h"

static bool exec_openrc(pam_handle_t *pamh, const char *runlevel) {
	char *cmd = NULL;
	const char *username;
	struct passwd *pw = NULL;

	if (pam_get_user(pamh, &username, "username:") != PAM_SUCCESS)
		return false;
	pw = getpwnam(username);
	if (!pw)
		return false;

	elog(LOG_INFO, "Executing %s runlevel for user %s", runlevel, username);

	xasprintf(&cmd, "openrc --user %s", runlevel);
	switch (fork()) {
		case 0:
			setgid(pw->pw_gid);
			setuid(pw->pw_uid);

			execl(pw->pw_shell, "-", "-c", cmd, NULL);

			free(cmd);
			return false;
			break;
		case -1:
			free(cmd);
			return false;
			break;
	}
	wait(NULL);
	free(cmd);
	return true;
}

PAM_EXTERN int pam_sm_open_session(pam_handle_t *pamh, int flags, int argc, const char **argv) {
	(void)flags;
	(void)argc;
	(void)argv;

	setenv("EINFO_LOG", "openrc-pam", 1);
	elog(LOG_INFO, "Opening openrc session");

	setenv("RC_PAM_STARTING", "YES", true);
	if (exec_openrc(pamh, "default")) {
		elog(LOG_INFO, "Openrc session opened");
		unsetenv("RC_PAM_STARTING");
		unsetenv("EINFO_LOG");
		return PAM_SUCCESS;
	} else {
		elog(LOG_ERR, "Failed to open session");
		unsetenv("RC_PAM_STARTING");
		unsetenv("EINFO_LOG");
		return PAM_SESSION_ERR;
	}
}

PAM_EXTERN int pam_sm_close_session(pam_handle_t *pamh, int flags, int argc, const char **argv) {
	(void)flags;
	(void)argc;
	(void)argv;

	setenv("EINFO_LOG", "openrc-pam", 1);
	elog(LOG_INFO, "Closing openrc session");

	setenv("RC_PAM_STOPPING", "YES", true);
	if (exec_openrc(pamh, "none")) {
		elog(LOG_INFO, "Openrc session closed");
		unsetenv("RC_PAM_STOPPING");
		unsetenv("EINFO_LOG");
		return PAM_SUCCESS;
	} else {
		elog(LOG_ERR, "Failed to close session");
		unsetenv("RC_PAM_STOPPING");
		unsetenv("EINFO_LOG");
		return PAM_SESSION_ERR;
	}
}
