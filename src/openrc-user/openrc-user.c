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

#ifdef HAVE_PAM
#include <security/pam_appl.h>
static struct pam_conv conv = { NULL, NULL };
static pam_handle_t *pamh = NULL;
#endif

#include "helpers.h"
#include "rc.h"

const struct passwd *user;
char *pidfile;

static void cleanup(void) {
#ifdef HAVE_PAM
	if (pamh) {
		int rc;
		if ((rc = pam_close_session(pamh, PAM_SILENT)) != PAM_SUCCESS)
			elog(LOG_ERR, "Failed to close session: %s", pam_strerror(pamh, rc));
		if ((rc = pam_setcred(pamh, PAM_DELETE_CRED)) != PAM_SUCCESS)
			elog(LOG_ERR, "Failed to delete user's credentials: %s", pam_strerror(pamh, rc));
		pam_end(pamh, rc);
	}
#endif

	unlink(pidfile);
	free(pidfile);
}

static bool do_setup(const char *username) {
	char *logname;
	int nullfd;

	xasprintf(&logname, "openrc-user[%s]", username);
	setenv("EINFO_LOG", logname, true);
	free(logname);

	if (!(user = getpwnam(username))) {
		elog(LOG_ERR, "getpwnam failed: %s", strerror(errno));
		return false;
	}

	nullfd = open("/dev/null", O_RDWR);
	dup2(nullfd, STDIN_FILENO);
	close(nullfd);

	return true;
}

static bool check_pidfile(bool start) {
	struct stat buf;
	FILE *file;

	xasprintf(&pidfile, "%s/users/%s", rc_svcdir(), user->pw_name);
	stat(pidfile, &buf);
	if (stat(pidfile, &buf) == 0 && buf.st_size > 0 && (file = fopen(pidfile, "r"))) {
		pid_t pid = 0;
		if (fscanf(file, "%d", &pid) != 1 || kill(pid, start ? SIGUSR1 : SIGUSR2) == -1) {
			elog(LOG_ERR, "Failed to signal pid %d, resetting.", pid);
			exit(1);
		}
		return true;
	}

	if (!(file = fopen(pidfile, "w"))) {
		elog(LOG_ERR, "Failed to open pidfile %s: %s", pidfile, strerror(errno));
		exit(1);
	}

	fprintf(file, "%d\n", getpid());
	fclose(file);

	return false;
}

static void do_openrc(bool start) {
	pid_t child;
	char *cmd;

	switch ((child = fork())) {
	case 0:
		if (setgid(user->pw_gid) == -1 || setuid(user->pw_uid) == -1) {
			elog(LOG_ERR, "Failed to drop permissions to user.");
			exit(1);
		}

		setenv("HOME", user->pw_dir, true);
		setenv("SHELL", user->pw_shell, true);

		xasprintf(&cmd, "%s %s", RC_LIBEXECDIR "/sh/openrc-user.sh", start ? "start" : "stop");
		execl(user->pw_shell, "-", "-c", cmd, NULL);

		elog(LOG_ERR, "Failed to execl '%s - -c %s': %s.", user->pw_shell, cmd, strerror(errno));
		exit(1);
	case -1:
		exit(1);
	default:
		break;
	}

	waitpid(child, NULL, 0);
}

static void do_wait(void) {
	size_t logins = 1;
	sigset_t set;
	int sig;

	sigemptyset(&set);
	sigaddset(&set, SIGUSR1);
	sigaddset(&set, SIGUSR2);
	sigaddset(&set, SIGTERM);

	sigprocmask(SIG_SETMASK, &set, NULL);

	while (logins > 0) {
		sigwait(&set, &sig);
		switch (sig) {
		case SIGUSR1:
			logins++;
			break;
		case SIGUSR2:
			logins--;
			break;
		case SIGTERM:
		case SIGKILL:
			return;
		default:
			break;
		}
	}

	return;
}

#ifdef HAVE_PAM
static void open_session(void) {
	bool pam_session = false;
	int rc;

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
		return;
	}

	return;
}
#endif

int main(int argc, char **argv) {
	bool start;

	if (argc < 3) {
		fprintf(stderr, "%s: Not enough arguments.\n", argv[0]);
		return 1;
	}

	if (strcmp(argv[2], "start") == 0) {
		start = true;
	} else if (strcmp(argv[2], "stop") == 0) {
		start = false;
	} else {
		fprintf(stderr, "%s: Invalid argument %s.\n", argv[0], argv[2]);
		return 1;
	}

	if (!do_setup(argv[1]))
		return 1;

	if (check_pidfile(start))
		return 0;

	atexit(cleanup);

	/* we need to initgroups before pam is setup. */
	if (initgroups(user->pw_name, user->pw_gid) == -1)
		return 1;

#ifdef HAVE_PAM
	open_session();
#endif

	do_openrc(true);
	do_wait();
	do_openrc(false);

	return 0;
}
