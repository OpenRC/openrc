#ifdef __linux__
#include <sys/capability.h>
#endif
#include <sys/file.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <grp.h>
#include <poll.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>

#ifdef HAVE_PAM
#include <security/pam_appl.h>

/* We are not supporting authentication conversations */
static struct pam_conv conv = { NULL, NULL};
#endif

#include <einfo.h>
#include <rc.h>

#include "daemon.h"

#include "misc.h"
#include "timeutils.h"
#include "schedules.h"
#include "rc_exec.h"

extern const char *applet;
extern int signal_pipe[2];
int socket_fd = -1;
bool notified = false;

static struct passwd *
set_user(char *username)
{
	char *user, *group = username;
	struct passwd *pw;
	struct group *gr = NULL;
	int id;

	user = strsep(&group, ":");

	if (sscanf(user, "%d", &id) == 1)
		pw = getpwuid(id);
	else
		pw = getpwnam(user);

	if (group) {
		if (sscanf(group, "%d", &id) == 1)
			gr = getgrgid(id);
		else
			gr = getgrnam(group);

		if (!gr) {
			eerror("%s: group '%s' not found", applet, user);
			return NULL;
		}
	}

	if (!user) {
		eerror("%s: user '%s' not found", applet, user);
		return NULL;
	}

	setenv("USER", pw->pw_name, true);
	setenv("LOGNAME", pw->pw_name, true);
	setenv("HOME", pw->pw_dir, true);

	initgroups(pw->pw_name, pw->pw_gid);

	if (setgid(gr ? gr->gr_gid : pw->pw_gid)) {
		eerror("%s: unable to set groupid to %d", applet, gr ? gr->gr_gid : pw->pw_gid);
		return NULL;
	}

#ifdef __linux__
	if (cap_setuid(pw->pw_uid)) {
#else
	if (setuid(pw->pw_uid)) {
#endif
		eerror("%s: unable to set userid to %d", applet, pw->pw_uid);
		return NULL;
	}

	/* Close any fd's to the passwd database */
	endpwent();

	return 0;
}

static pid_t
spawn_child(int respawn_count, const char *svcname, char **argv)
{
	pid_t child_pid;

	time_t start_time = time(NULL);
	char start_time_string[20];

	struct sigaction sa = { .sa_handler = SIG_DFL };
	sigset_t signals;

	sigaction(SIGCHLD, &sa, NULL);
	sigaction(SIGTERM, &sa, NULL);
	sigemptyset(&signals);
	sigprocmask(SIG_SETMASK, &signals, NULL);

	from_time_t(start_time_string, start_time);

	rc_service_value_set(svcname, "start_time", start_time_string);
	rc_service_value_fmt(svcname, "start_count", "%d", respawn_count);

	switch ((child_pid = fork())) {
	case 0:
		child_process(svcname, argv);
	case -1:
		eerrorx("fork failed");
	default:
		rc_service_value_fmt(svcname, "child_pid", "%d", child_pid);
	}

	return child_pid;
}

static
bool healthy(const char *svcname)
{
	pid_t pid = exec_service(svcname, "healthcheck");
	int status = rc_waitpid(pid);
	if (status != -1 && WIFEXITED(status) && WEXITSTATUS(status) == 0)
		return true;

	exec_service(svcname, "unhealthy");
	return false;
}

static int
setup_control(void)
{
	int fd;

	if (mkfifo("control", 0700) == -1 && errno != EEXIST)
		return -1;

	if ((fd = open("control", O_RDONLY | O_NONBLOCK)) == -1)
		return -1;

	if (flock(fd, LOCK_EX) == -1)
		return -1;

	return fd;
}

static int
setup_notify(const char *svcname)
{
	int fd;
	char *path;
	union {
		struct sockaddr header;
		struct sockaddr_un unix;
	} addr = { .unix = { .sun_family = AF_UNIX } };
	int written = snprintf(addr.unix.sun_path, sizeof(addr.unix.sun_path), "notify");

	if (written >= (int)sizeof(addr.unix.sun_path))
		eerrorx("%s: socket name '%s' too long.", applet, applet);

	if ((fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1)
		eerrorx("%s: socket: %s", applet, strerror(errno));
	if (bind(fd, &addr.header, sizeof(addr.unix)) == -1)
		goto err;
	if (listen(fd, 5) == -1)
		goto err;

	xasprintf(&path, "NOTIFY_SOCKET=\"%s/daemons/%s/notify\"", rc_svcdir(), svcname);
	putenv(path);
	return fd;

err:
	close(fd);
	return -1;
}

static bool
setup_supervisor(const char *svcname)
{
	sigset_t signals;
	int dirfd;

	if (pipe2(signal_pipe, O_CLOEXEC | O_NONBLOCK) == -1)
		return false;

	sigfillset(&signals);
	sigdelset(&signals, SIGCHLD);
	sigprocmask(SIG_SETMASK, &signals, NULL);

	mkdirat(rc_dirfd(RC_DIR_DAEMONS), svcname, 0755);
	if ((dirfd = openat(rc_dirfd(RC_DIR_DAEMONS), svcname, O_RDONLY | O_DIRECTORY)) == -1)
		return false;

	if (fchdir(dirfd) == -1)
		return false;
	if (pipe2(signal_pipe, O_CLOEXEC | O_NONBLOCK) == -1)
		return false;

	close(dirfd);

	return true;
}

static void
handle_notify_socket(struct notify *notify, int socket)
{
	char buf[BUFSIZ];
	size_t count = read(socket, buf, sizeof(buf) - 1);
	if (count <= 0)
		return;

	buf[count] = '\0';
	if (notify->type != NOTIFY_SOCKET || socket_fd == -1)
		return;
	if (strstr(buf, "READY=1")) {
		close(socket_fd);
		socket_fd = -1;
	}
}

RC_NORETURN void
supervise(const char *svcname)
{
	int argc; char **argv;
	struct notify notify;
	bool chuser = false;
	pid_t child_pid;
	char *option;
	int count;

	int64_t next_healthcheck = 0;
	int healthcheck_delay = 0;
	int healthcheck_timer = 0;
	int timeout = -1;

	int64_t first_respawn = 0;
	int respawn_period = 0;
	int respawn_count = 0;
	int respawn_delay = 0;
	int respawn_max = 10;

#ifdef HAVE_PAM
	pam_handle_t *pamh;
#endif

	enum { CONTROL, CHILD, NOTIFY, READY };

	struct pollfd pfds[] = {
		[CONTROL] = { .events = POLLIN },
		[NOTIFY] = { .events = POLLIN },
		[CHILD] = { .events = POLLIN },
		[READY] = { .events = POLLIN },
	};
	nfds_t nfds = ARRAY_SIZE(pfds) - 1;

	xasprintf(&option, "supervise[%s]", svcname);
	openlog(option, LOG_PID, LOG_DAEMON);
	free(option);

	if (!(setup_supervisor(svcname)))
		exit(EXIT_FAILURE);

	if (!(option = rc_service_value_get(svcname, "argc")) || sscanf(option, "%d", &argc) != -1)
		exit(EXIT_FAILURE);
	free(option);

	argv = xmalloc((argc * sizeof(*argv)) + 1);
	if (!(argv[argc] = rc_service_value_get(svcname, "argv")))
		exit(EXIT_FAILURE);

	for (int i = 0; i < argc; i++)
		argv[i] = strsep(&argv[argc], "\n");
	argv[argc] = NULL; /* sanity check */

	if ((option = rc_service_value_get(svcname, "healthcheck-timer"))) {
		if (sscanf(option, "%d", &healthcheck_timer) == -1)
			exit(EXIT_FAILURE);
		timeout = healthcheck_timer;
	}

	if ((option = rc_service_value_get(svcname, "healthcheck-delay"))) {
		if (sscanf(option, "%d", &healthcheck_delay) == -1)
			exit(EXIT_FAILURE);
		timeout = healthcheck_delay;
	}

	/* TODO: use activation environment */
	if ((option = rc_service_value_get(svcname, "env"))) {
		char *env;
		while ((env = strsep(&option, "\n")))
			putenv(env);
	}

	/*
	 * TODO: handle pidfile daemon and daemons that background themselves.
	 */

	respawn_period = parse_duration(rc_service_value_get(svcname, "respawn-period"));
	respawn_delay = parse_duration(rc_service_value_get(svcname, "respawn-delay"));
	if ((option = rc_service_value_get(svcname, "respawn-max")) && sscanf(option, "%d", &respawn_max) == -1)
		exit(EXIT_FAILURE);

	pfds[CONTROL].fd = setup_control();
	pfds[NOTIFY].fd = setup_notify(svcname);

	if ((option = rc_service_value_get(svcname, "notify"))) {
		notify = notify_parse(svcname, option);
		if (notify.type == NOTIFY_FD)
			nfds++;
		free(option);
	}

	if (!rc_is_user()) {
		struct passwd *pw;
		int pamr;

		if ((option = rc_service_value_get(svcname, "user")))
			pw = set_user(option);
		else
			pw = getpwuid(getuid());

		if (!pw)
			exit(EXIT_FAILURE);

#ifdef HAVE_PAM
		pamr = pam_start("supervise-daemon", pw->pw_name, &conv, &pamh);

		if (pamr == PAM_SUCCESS)
			pam_acct_mgmt(pamh, PAM_SILENT);
		if (pamr == PAM_SUCCESS)
			pam_open_session(pamh, PAM_SILENT);

		if (pamr != PAM_SUCCESS)
			eerrorx("%s: pam error: %s", applet, pam_strerror(pamh, pamr));

		for (char **env = pam_getenvlist(pamh); env && *env; env++) {
			/* Don't add strings unless they set a var */
			if (strchr(*env, '='))
				putenv(xstrdup(*env));
			else
				unsetenv(*env);
		}

		chuser = true;
#endif
	}

	child_pid = spawn_child(respawn_count, svcname, argv);

	if (healthcheck_timer)
		next_healthcheck = tm_now() + healthcheck_timer;
	for (;;) {
		switch (poll(pfds, nfds, timeout)) {
		case 0:
			if (!healthy(svcname)) {
				kill(-child_pid, SIGTERM);
				if (poll(&pfds[child_pid], 1, 5) <= 0)
					kill(-child_pid, SIGKILL);
				child_pid = spawn_child(respawn_count++, svcname, argv);
			}
			if (healthcheck_timer) {
				timeout = healthcheck_timer;
				next_healthcheck = tm_now() + timeout;
			}
		case -1:
			continue;
		}

		if (pfds[CONTROL].revents & POLLIN) {
			char buf[BUFSIZ];
			char sigbuf[20];
			if ((count = read(pfds[CONTROL].fd, buf, sizeof(buf) - 1)) > 0) {
				int sig;
				buf[count] = '\0';
				if (strcmp(buf, "stop") == 0)
					goto stop;
				if (sscanf(buf, "signal %d", &sig) == 1 && sig > 0 && sig <= NSIG)
					kill(child_pid, sig);
				else if (sscanf(buf, "signal %19s", sigbuf) == 1) {
					/* TODO: don't eerrorx internally */
					sig = parse_signal(applet, sigbuf);
					if (sig > 0 && sig <= NSIG)
						kill(child_pid, sig);
				}
			}
		}

		if (pfds[NOTIFY].revents & POLLIN)
			handle_notify_socket(&notify, pfds[NOTIFY].fd);

		if (pfds[READY].revents & POLLIN) {
			char buf[BUFSIZ];
			if ((count = read(pfds[NOTIFY].fd, buf, sizeof(buf) - 1)) > 0) {
				if (memchr(buf, '\n', count)) {
					close(socket_fd);
					socket_fd = -1;
					nfds--;
				}
			}
		}

		if (pfds[CHILD].revents & POLLIN) {
			siginfo_t info;

			if ((count = read(pfds[CHILD].fd, &info, sizeof(info))) != sizeof(info))
				continue;

			if (waitpid(info.si_pid, NULL, WNOHANG) <= 0 || info.si_pid != child_pid)
				continue;

			if ((respawn_period > 0) && (tm_now() - first_respawn > respawn_period))
				respawn_count = first_respawn = 0;
			else
				respawn_count++;

			tm_sleep(respawn_delay, TM_NO_EINTR);

			child_pid = spawn_child(respawn_count, svcname, argv);
		}

		if (healthcheck_timer)
			timeout = next_healthcheck - tm_now();
	}

stop:
	kill(-child_pid, SIGTERM);
	if (poll(&pfds[CHILD], 1, 5) <= 0)
		kill(-child_pid, SIGKILL);

#ifdef HAVE_PAM
	if (chuser)
		pam_close_session(pamh, PAM_SILENT);
#endif

	unlinkat(rc_dirfd(RC_DIR_DAEMONS), svcname, 0);
	exit(EXIT_SUCCESS);
}
