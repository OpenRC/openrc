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

static bool exec_openrc(pam_handle_t *pamh, const char *runlevel) {
	char *cmd = NULL;
	const char *username;
	struct passwd *pw = NULL;

	if (pam_get_user(pamh, &username, "username:") != PAM_SUCCESS)
		return false;
	pw = getpwnam(username);
	if (!pw)
		return false;

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

	setenv("RC_PAM_STARTING", "YES", true);
	if (exec_openrc(pamh, "default")) {
		unsetenv("RC_PAM_STARTING");
		return PAM_SUCCESS;
	} else {
		unsetenv("RC_PAM_STARTING");
		return PAM_SESSION_ERR;
	}
}

PAM_EXTERN int pam_sm_close_session(pam_handle_t *pamh, int flags, int argc, const char **argv) {
	(void)flags;
	(void)argc;
	(void)argv;

	setenv("RC_PAM_STOPPING", "YES", true);
	if (exec_openrc(pamh, "none")) {
		unsetenv("RC_PAM_STOPPING");
		return PAM_SUCCESS;
	} else {
		unsetenv("RC_PAM_STOPPING");
		return PAM_SESSION_ERR;
	}
}
