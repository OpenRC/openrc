#include <einfo.h>
#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <pwd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <syslog.h>
#include <unistd.h>
#include <signal.h>
#include <spawn.h>

#ifdef HAVE_PAM
#include <security/pam_appl.h>
#endif

#define USER_SH RC_LIBEXECDIR "/sh/openrc-user.sh "

#include "helpers.h"
#include "rc.h"

static bool spawn_openrc(const struct passwd *user, bool start) {
	int status = 0;
	pid_t pid;

	switch ((pid = fork())) {
	case 0:
		if (setgid(user->pw_gid) == -1 || setuid(user->pw_uid) == -1) {
			elog(LOG_ERR, "Failed to set user ids: %s", strerror(errno));
			exit(-1);
		}

		execl(user->pw_shell, "-", "-c", start ? USER_SH "start" : USER_SH "stop", NULL);
		elog(LOG_ERR, "execl: %s", strerror(errno));
		exit(-1);
		break;

	case -1:
		elog(LOG_ERR, "fork: %s", strerror(errno));
		return false;

	default:
		waitpid(pid, &status, 0);
		break;
	}

	return status == 0;
}

int main(int argc, char **argv) {
	const struct passwd *user;
	sigset_t sigmask;
	int sig, ret = 0;
	char *log;
#ifdef HAVE_PAM
	struct pam_conv conv = { NULL, NULL };
	pam_handle_t *pamh = NULL;
	bool pam_session = false;
	int rc;
#endif

	setenv("EINFO_LOG", "openrc-user", true);

	if (argc < 2 || argc > 2) {
		elog(LOG_ERR, "Invalid usaged. %s <username>", argv[0]);
		return -1;
	}

	/* We can't rely on the supervisor to drop perms since
	 * pam modules might need root perms. */
	if (!(user = getpwnam(argv[1]))) {
		elog(LOG_ERR, "Invalid username %s.", argv[1]);
		return -1;
	}

	setenv("USER", user->pw_name, true);
	setenv("LOGNAME", user->pw_name, true);
	setenv("HOME", user->pw_dir, true);
	setenv("SHELL", user->pw_shell, true);

	initgroups(user->pw_name, user->pw_gid);

	xasprintf(&log, "openrc-user.%s", user->pw_name);
	setenv("EINFO_LOG", log, true);
	free(log);

#ifdef HAVE_PAM
	if ((rc = pam_start("openrc-user", user->pw_name, &conv, &pamh)) != PAM_SUCCESS)
		elog(LOG_ERR, "Failed to start pam: %s", pam_strerror(pamh, rc));
	else if ((rc = pam_open_session(pamh, PAM_SILENT)) != PAM_SUCCESS)
		elog(LOG_ERR, "Failed to open session: %s", pam_strerror(pamh, rc));
	else
		pam_session = true;

	for (char **env = pam_getenvlist(pamh); env && *env; env++) {
		if (strchr(*env, '='))
			putenv(xstrdup(*env));
		else
			unsetenv(*env);
	}

	if (!pam_session && pamh) {
		pam_end(pamh, rc);
		pamh = NULL;
	}
#endif

	if (!spawn_openrc(user, true)) {
		ret = -1;
		goto out;
	}

	sigfillset(&sigmask);
	sigprocmask(SIG_SETMASK, &sigmask, NULL);

	sigemptyset(&sigmask);
	sigaddset(&sigmask, SIGTERM);

	sigwait(&sigmask, &sig);

	if (!spawn_openrc(user, false))
		ret = -1;

out:
#ifdef HAVE_PAM
	if (pamh) {
		if ((rc = pam_close_session(pamh, PAM_SILENT)) != PAM_SUCCESS)
			elog(LOG_ERR, "Failed to close session: %s", pam_strerror(pamh, rc));
		pam_end(pamh, rc);
	}
#endif

	return ret;
}
