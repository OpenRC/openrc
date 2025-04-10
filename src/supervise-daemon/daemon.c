#ifdef __linux__
#include <sys/capability.h>
#include <sys/prctl.h>
#endif
#include <sys/resource.h>

#include <errno.h>
#include <pwd.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

#include <einfo.h>
#include <rc.h>

#include "rc_exec.h"
#include "helpers.h"
#include "misc.h"

#if !defined(SYS_ioprio_set) && defined(__NR_ioprio_set)
# define SYS_ioprio_set __NR_ioprio_set
#endif
#if !defined(__DragonFly__)
static inline int ioprio_set(int which RC_UNUSED, int who RC_UNUSED,
			     int ioprio RC_UNUSED)
{
#ifdef SYS_ioprio_set
	return syscall(SYS_ioprio_set, which, who, ioprio);
#else
	return 0;
#endif
}
#endif

extern const char *applet;

static bool
set_nicelevel(const char *option)
{
	int nicelevel;
	FILE *fp;

	if (sscanf(option, "%d", &nicelevel) != 1 || setpriority(PRIO_PROCESS, getpid(), nicelevel)) {
		syslog(LOG_ERR, "setpriority %d: %s", nicelevel, strerror(errno));
		return false;
	}

	if ((fp = fopen("/proc/self/autogroup", "r+"))) {
		fprintf(fp, "%d\n", nicelevel);
		fclose(fp);
	} else if (errno != ENOENT) {
		syslog(LOG_ERR, "autogroup nice %d: %s", nicelevel, strerror(errno));
		return false;
	}

	return true;
}

static bool
set_ionice(const char *option)
{
	int ionicec, ioniced;
	if (sscanf(option, "%d:%d", &ionicec, &ioniced) == 0) {
		syslog(LOG_ERR, "invalid ionice '%s'", optarg);
		return false;
	}
	if (ionicec == 0)
		ioniced = 0;
	else if (ionicec == 3)
		ioniced = 7;
	ionicec <<= 13; /* class shift */

	if (ioprio_set(1, getpid(), ionicec | ioniced) == -1) {
		syslog(LOG_ERR, "ioprio_set %d %d: %s", ionicec, ioniced, strerror(errno));
		return false;
	}
	return true;
}

static bool
set_oom_score(const char *option)
{
	int score;
	FILE *fp;

	if (sscanf(option, "%d", &score) != 1) {
		syslog(LOG_ERR, "invalid oom-score-adj '%s'", option);
		return false;
	}

	if (!(fp = fopen("/proc/self/oom_score_adj", "w"))) {
		syslog(LOG_ERR, "oom_score_adj %d: %s", score, strerror(errno));
		return false;
	}

	fprintf(fp, "%d\n", score);
	fclose(fp);

	return true;
}

static bool
set_capabilities(const char *option)
{
#ifdef __linux__
	cap_iab_t cap_iab;

	if (!(cap_iab = cap_iab_from_text(option)) || cap_iab_set_proc(cap_iab) != 0 || cap_free(cap_iab) != 0) {
		syslog(LOG_ERR, "failed to set capabilities");
		return false;
	}

	return true;
#endif

	errno = ENOSYS;
	return false;
}

static bool
set_secbits(const char *option)
{
#ifdef __linux__
	unsigned secbits;
	char *end;

	if (*option == '\0') {
		syslog(LOG_ERR, "secbits are empty");
		return false;
	}

	secbits = strtoul(option, &end, 0);
	if (option == end) {
		syslog(LOG_ERR, "could not parse secbits: %s", option);
		return false;
	}

	if (cap_set_secbits(secbits) < 0) {
		syslog(LOG_ERR, "could not set securebits to 0x%x: %s", secbits, strerror(errno));
		return false;
	}

	return true;
#endif

	errno = ENOSYS;
	return false;
}

static bool
set_no_new_privs(const char *option)
{
	if (!rc_yesno(option))
		return true;
#ifdef PR_SET_NO_NEW_PRIVS
	if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0) == -1) {
		syslog(LOG_ERR, "prctl: %s", strerror(errno));
		return false;
	}

	return true;
#endif

	errno = ENOSYS;
	return false;
}

static bool
set_umask(const char *option)
{
	mode_t mask = 022;
	if (parse_mode(&mask, option)) {
		syslog(LOG_ERR, "%s: invalid mode '%s'", applet, optarg);
		return false;
	}

	umask(mask);
	return true;
}

static bool
set_scheduler(RC_UNUSED const char *option)
{
	/* TODO */
	return true;
}

static char *expand_home(const char *path)
{
	char *expanded = NULL;
	struct passwd *pw;
	const char *home;
	size_t size;
	FILE *fp;

	if (path[0] == '\\' && path[1] == '~')
		return xstrdup(path + 1);
	else if (path[0] != '~')
		return xstrdup(path);

	fp = xopen_memstream(&expanded, &size);
	path++;

	if (path[0] == '/' || path[0] == '\0') {
		if ((home = getenv("HOME"))) {
			fputs(home, fp);
		} else if ((pw = getpwuid(getuid()))) {
			fputs(pw->pw_dir, fp);
		} else {
			syslog(LOG_ERR, "getpwuid: %s", strerror(errno));
			goto err;
		}
	} else {
		char *username;

		if ((home = strchr(path, '/'))) {
			username = xstrndup(path, home - path);
			path = home + 1;
		} else {
			username = xstrdup(path);
			path = NULL;
		}

		if (!(pw = getpwnam(username))) {
			syslog(LOG_ERR, "user %s not found", username);
			goto err;
		}
		fputs(pw->pw_dir, fp);
		free(username);
	}

	if (path)
		fputs(path, fp);

	xclose_memstream(fp);
	return expanded;

err:
	xclose_memstream(fp);
	free(expanded);
	return NULL;
}

static bool
do_chdir(const char *option)
{
	char *expand_path = expand_home(option);
	if (!expand_path)
		return false;
	if (chdir(expand_path) == -1) {
		syslog(LOG_ERR, "chdir(%s): %s", expand_path, strerror(errno));
		return false;
	}
	free(expand_path);
	return true;
}

static bool
do_chroot(const char *option)
{
	char *expand_path = expand_home(option);
	if (!expand_path)
		return false;
	if (chroot(expand_path) == -1) {
		syslog(LOG_ERR, "chroot(%s): %s", expand_path, strerror(errno));
		return false;
	}
	free(expand_path);
	return true;
}

enum open_type { READONLY, CREATE, REDIRECT };

static bool
do_open(const char *option, enum open_type type, int target)
{
	int flags = type == CREATE ? O_WRONLY | O_CREAT | O_APPEND : O_RDONLY;
	int fd;

	if (type == REDIRECT) {
		if ((fd = rc_pipe_command(option)) == -1) {
			syslog(LOG_ERR, "failed to pipe command to %s", option);
			return false;
		}
	} else {
		if ((fd = open(option, flags, S_IRUSR | S_IWUSR)) == -1) {
			syslog(LOG_ERR, "failed to open %s", option);
			return false;
		}
	}

	if (dup2(fd, target) != -1) {
		syslog(LOG_ERR, "dup2: %s", strerror(errno));
		return false;
	}

	return true;
}

RC_NORETURN void
child_process(const char *svcname, struct notify *notify, char **argv)
{
	/* the order of the options matter, as they're applied
	 * in order so earlier options affect later ones! */
	static const struct {
		const char * const name;
		bool (*handler)(const char *option);
		enum open_type type;
		int target;
	} options[] = {
		{ "umask",              set_umask,        0, 0 },
		{ "nicelevel",          set_nicelevel,    0, 0 },
		{ "ionice",             set_ionice,       0, 0 },
		{ "oom-score-adj",      set_oom_score,    0, 0 },
		{ "scheduler",          set_scheduler,    0, 0 },
		{ "scheduler-priority", set_scheduler,    0, 0 },
		{ "capabilities",       set_capabilities, 0, 0 },
		{ "secbits",            set_secbits,      0, 0 },
		{ "no-new-privs",       set_no_new_privs, 0, 0 },
		{ "chroot",             do_chroot,        0, 0 },
		{ "chdir",              do_chdir,         0, 0 },

		{ "stdin",         NULL, READONLY, STDIN_FILENO  },
		{ "stdout",        NULL, CREATE,   STDOUT_FILENO },
		{ "stderr",        NULL, CREATE,   STDERR_FILENO },
		{ "stdout-logger", NULL, REDIRECT, STDOUT_FILENO },
		{ "stderr-logger", NULL, REDIRECT, STDERR_FILENO },
	};

	setsid();

	for (size_t i = 0; i < ARRAY_SIZE(options); i++) {
		char *option;
		if (!(option = rc_service_value_get(svcname, options[i].name)))
			continue;
		if (options[i].handler ? !options[i].handler(option)
				: do_open(options[i].name, options[i].type, options[i].target))
			exit(EXIT_FAILURE);
		free(option);
	}

	cloexec_fds_from(3);

	if (notify->type == NOTIFY_FD && dup2(notify->pipe[1], notify->fd) == -1) {
		syslog(LOG_ERR, "Failed to initialize ready fd.");
		exit(EXIT_FAILURE);
	}

	execvp(*argv, argv);

	syslog(LOG_ERR, "%s: failed to exec '%s': %s", applet, *argv, strerror(errno));
	exit(EXIT_FAILURE);
}
