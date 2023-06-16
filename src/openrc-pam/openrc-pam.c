#include <librc.h>
#include <pwd.h>
#include <security/pam_appl.h>
#include <security/pam_modules.h>
#include <stdbool.h>
#include <stdio.h>
#include <syslog.h>
#include <unistd.h>

#include "einfo.h"

static bool exec_openrc(pam_handle_t *pamh, const char *runlevel, bool lock) {
	char *cmd = NULL;
	const char *username;
	struct passwd *pw = NULL;
	char **envlist;
	char **env;

	envlist = pam_getenvlist(pamh);

	if (pam_get_user(pamh, &username, "username:") != PAM_SUCCESS)
		return false;
	pw = getpwnam(username);
	if (!pw)
		return false;

	elog(LOG_INFO, "Executing %s runlevel for user %s", runlevel, username);

	xasprintf(&cmd, "openrc --user %s %s", lock ? "--lock" : "--unlock", runlevel);
	switch (fork()) {
		case 0:
			setgid(pw->pw_gid);
			setuid(pw->pw_uid);

			execle(pw->pw_shell, "-", "-c", cmd, NULL, envlist);

			free(cmd);
			return false;
			break;
		case -1:
			free(cmd);
			return false;
			break;
	}
	wait(NULL);

	for (env = envlist; *env; env++)
		free(*env);
	free(envlist);
	free(cmd);
	return true;
}

PAM_EXTERN int pam_sm_open_session(pam_handle_t *pamh, int flags, int argc, const char **argv) {
	const char *runlevel = argc > 0 ? runlevel = argv[0] : "default";
	(void)flags;

	setenv("EINFO_LOG", "openrc-pam", 1);
	elog(LOG_INFO, "Opening openrc session");

	if (exec_openrc(pamh, runlevel, true)) {
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
	const char *runlevel = argc > 1 ? argv[1] : "none";
	(void)flags;

	setenv("EINFO_LOG", "openrc-pam", 1);
	elog(LOG_INFO, "Closing openrc session");

	if (exec_openrc(pamh, runlevel, false)) {
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
