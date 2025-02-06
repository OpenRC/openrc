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

enum setup_mode {
	CLIENT,
	SERVER,
};

static const struct passwd *user;
static size_t logins = 1;
static char *fifopath;
static bool timeout;

static void handle_signal(int sig) {
	if (sig == SIGALRM)
		timeout = true;
}

static void cleanup(void) {
#ifdef HAVE_PAM
	if (pamh) {
		int rc;
		if ((rc = pam_close_session(pamh, PAM_SILENT)) != PAM_SUCCESS)
			elog(LOG_ERR, "Failed to close session: %s", pam_strerror(pamh, rc));
		pam_end(pamh, rc);
	}
#endif

	if (exists(fifopath))
		unlink(fifopath);
	free(fifopath);
}

static enum setup_mode do_setup(const char *username) {
	struct sigaction sa = { .sa_handler = handle_signal };
	char *logname;
	int nullfd;

	xasprintf(&logname, "openrc-user[%s]", username);
	setenv("EINFO_LOG", logname, true);
	free(logname);

	if (!(user = getpwnam(username))) {
		elog(LOG_ERR, "getpwnam failed: %s", strerror(errno));
		exit(EXIT_FAILURE);
	}

	sigaction(SIGALRM, &sa, NULL);

	setenv("USER", user->pw_name, true);
	setenv("HOME", user->pw_dir, true);
	setenv("SHELL", user->pw_shell, true);


	nullfd = open("/dev/null", O_RDWR);
	dup2(nullfd, STDIN_FILENO);
	close(nullfd);

	xasprintf(&fifopath, "%s/users/%s", rc_svcdir(), user->pw_name);
	if (mkfifo(fifopath, 0600) == -1 && errno != EEXIST) {
		elog(LOG_ERR, "mkfifo failed: %s", strerror(errno));
		exit(EXIT_FAILURE);
	}

	return errno == EEXIST ? CLIENT : SERVER;
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

		xasprintf(&cmd, "%s %s", RC_LIBEXECDIR "/sh/openrc-user.sh", start ? "start" : "stop");
		elog(LOG_INFO, "Running '%s - -c %s'", user->pw_shell, cmd);
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

static void open_session(void) {
#ifdef HAVE_PAM
	bool pam_session = false;
	int rc;

	if ((rc = pam_start("openrc-user", user->pw_name, &conv, &pamh)) != PAM_SUCCESS)
		elog(LOG_ERR, "Failed to start pam: %s", pam_strerror(pamh, rc));
	else if ((rc = pam_open_session(pamh, PAM_SILENT)) != PAM_SUCCESS)
		elog(LOG_ERR, "Failed to open session: %s", pam_strerror(pamh, rc));
	else
		pam_session = true;

	for (char **env = pam_getenvlist(pamh); env && *env; env++) {
		elog(LOG_INFO, "env: %s", *env);
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

	return;
}

int main(int argc, char **argv) {
	const char *username, *cmd, *service;
	FILE *fifo;

	/* Avoid recursing pam stacks */
	if ((service = getenv("PAM_SERVICE")) && strcmp(service, "openrc-user") == 0)
		return 0;

	setenv("EINFO_LOG", "openrc-user[??]", true);

	switch (argc) {
	case 3:
		username = argv[1];
		cmd = argv[2];
		break;
#ifdef HAVE_PAM
	case 1:
		if (!(username = getenv("PAM_USER"))) {
			elog(LOG_ERR, "PAM_USER unset and no arguments given.");
			return 1;
		} if (!(cmd = getenv("PAM_TYPE"))) {
			elog(LOG_ERR, "PAM_TYPE unset and no arguments given.");
			return 1;
		}

		if (strcmp(cmd, "open_session") == 0) {
			cmd = "start";
		} else if (strcmp(cmd, "close_session") == 0) {
			cmd = "stop";
		} else {
			elog(LOG_ERR, "Invalid PAM_TYPE.");
			return 1;
		}

		break;
#endif
	default:
		elog(LOG_ERR, "Invalid arguments.");
		for (int i = 0; i < argc; i++)
			elog(LOG_ERR, "argv[%d]: %s", i, argv[i]);
		return 1;
	}

	switch (do_setup(username)) {
	case CLIENT:
		elog(LOG_INFO, "Sending cmd %s to %s.", cmd, fifopath);
		alarm(5);
		fifo = fopen(fifopath, "w");

		if (!fifo && errno == EINTR && timeout) {
			elog(LOG_ERR, "Timed out.");
			return 1;
		}

		if (fputs(cmd, fifo) < 0)
			elog(LOG_ERR, "fputs failed.");
		free(fifopath);
		fclose(fifo);
		return 0;

	case SERVER:
		if (strcmp(cmd, "start") != 0) {
			elog(LOG_ERR, "Can't stop non-running server.");
			return 1;
		}

		setsid();
		if (fork() != 0)
			return 0;
		break;
	}

	atexit(cleanup);

	elog(LOG_INFO, "Opening session for %s.", username);

	/* we need to initgroups before pam is setup. */
	if (initgroups(user->pw_name, user->pw_gid) == -1) {
		elog(LOG_ERR, "initgroups failed.");
		return 1;
	}

	open_session();

	do_openrc(true);

	elog(LOG_INFO, "Listening fifo on %s.", fifopath);
	for (;;) {
		char buf[BUFSIZ];
		size_t count;

		if (!(fifo = fopen(fifopath, "r"))) {
			if (errno != EINTR)
				elog(LOG_ERR, "fopen failed: %s", strerror(errno));
			continue;
		}

		count = fread(buf, sizeof(char), BUFSIZ - 1, fifo);
		buf[count] = '\0';
		fclose(fifo);

		if (count == 0)
			continue;

		elog(LOG_INFO, "Got command %s.", buf);

		if (strcmp(buf, "start") == 0)
			logins++;
		else if (strcmp(buf, "stop") == 0)
			logins--;
		else if (strcmp(buf, "shutdown") == 0)
			break;
		else
			elog(LOG_WARNING, "Ignoring unknown command %s.", buf);

		if (logins == 0)
			break;
	}

	do_openrc(false);

	return 0;
}
