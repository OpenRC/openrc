#include <getopt.h>
#include <poll.h>
#include <stdlib.h>

#include <rc.h>
#include <einfo.h>

#include "helpers.h"

enum {
  /* This has to come first so following values stay in the 0x100+ range. */
  LONGOPT_BASE = 0x100,

  LONGOPT_CAPABILITIES,
  LONGOPT_OOM_SCORE_ADJ,
  LONGOPT_NO_NEW_PRIVS,
  LONGOPT_SCHEDULER,
  LONGOPT_SCHEDULER_PRIO,
  LONGOPT_SECBITS,
  LONGOPT_STDERR_LOGGER,
  LONGOPT_STDOUT_LOGGER,
  LONGOPT_NOTIFY,
};

const char *applet;
const char getoptstring[] = "u:d:r:k:N:I:R:p:0:1:2:a:A:D:m:P:";
const struct option longopts[] = {
	{ "start",              no_argument,       NULL, 'S' },
	{ "stop",               no_argument,       NULL, 'K' },
	{ "signal",             no_argument,       NULL, 's' },

	/* daemon management options */
	{ "notify",             required_argument, NULL, LONGOPT_NOTIFY },
	{ "pidfile",            required_argument, NULL, 'p' },
	{ "retry",              required_argument, NULL, 'R' },

	{ "respawn-delay",      required_argument, NULL, 'D'},
	{ "respawn-max",        required_argument, NULL, 'm'},
	{ "respawn-period",     required_argument, NULL, 'P'},

	{ "healthcheck-timer",  required_argument, NULL, 'a'},
	{ "healthcheck-delay",  required_argument, NULL, 'A'},

	/* input/output options */
	{ "stdin",              required_argument, NULL, '0' },
	{ "stdout",             required_argument, NULL, '1' },
	{ "stderr",             required_argument, NULL, '2' },
	{ "stdout-logger",      required_argument, NULL, LONGOPT_STDOUT_LOGGER },
	{ "stderr-logger",      required_argument, NULL, LONGOPT_STDERR_LOGGER },

	/* environment setup options */
	{ "user",               required_argument, NULL, 'u' },
	{ "chdir",              required_argument, NULL, 'd' },
	{ "chroot",             required_argument, NULL, 'r' },
	{ "umask",              required_argument, NULL, 'k' },
	{ "nicelevel",          required_argument, NULL, 'N' },
	{ "ionice",             required_argument, NULL, 'I' },
	{ "capabilities",       required_argument, NULL, LONGOPT_CAPABILITIES   },
	{ "secbits",            required_argument, NULL, LONGOPT_SECBITS        },
	{ "oom-score-adj",      required_argument, NULL, LONGOPT_OOM_SCORE_ADJ  },
	{ "no-new-privs",       no_argument,       NULL, LONGOPT_NO_NEW_PRIVS   },
	{ "scheduler",          required_argument, NULL, LONGOPT_SCHEDULER      },
	{ "scheduler-priority", required_argument, NULL, LONGOPT_SCHEDULER_PRIO },

	/* legacy options, do nothing but warn for compat */
	{ "exec",               required_argument, NULL, 'x' },
	{ "group",              required_argument, NULL, 'g' },
	{ "interpreted",        no_argument,       NULL, 'i' },
	{ "progress",           no_argument,       NULL, 'P' },
	{ "make-pidfile",       no_argument,       NULL, 'm' },
	{ "wait",               no_argument,       NULL, 'w' },
	{ "background",         no_argument,       NULL, 'b' },
	{ "test",               no_argument,       NULL, 't' },
};

int main(int argc, char **argv) {
	const char *svcname, *start_opts[ARRAY_SIZE(longopts)] = {0};
	enum { START, STOP, SIGNAL } action = START;
	bool supervise_compat;
	struct pollfd ctrlfd;
	char *cmdline, *env;
	FILE *argv_stream;
	int idx, opt;
	size_t len;

	applet = basename_c(argv[0]);
	supervise_compat = strcmp(applet, "supervise-daemon") == 0;

	if (rc_yesno(getenv("RC_USER_SERVICES")))
		rc_set_user();

	while ((opt = getopt_long(argc, argv, getoptstring, longopts, &idx)) != -1)
		switch (opt) {
		case 'S': action = START;  break;
		case 'K': action = STOP;   break;
		case 's': action = SIGNAL; break;
		case 'i': case 'P': case 'm': case 'w': case 'b': case 't': case 'R':
			ewarn("DEPRECATED: -%c|--%s no longer takes any effect in the new supervise-daemon and"
					"start-stop-daemon, and will be an error in the future.\n", opt, longopts[idx].name);
			break;
		case 'x':
			ewarn("DEPRECATED: -x|--exec is no longer supported, please supply the command name as the first argument.");
			/* fall through */
		default:
			start_opts[idx] = longopts[idx].has_arg ? optarg : "true";
			break;
		}

	svcname = supervise_compat ? argv[optind++] : getenv("RC_SVCNAME");
	argc -= optind;
	argv += optind;

	if (!argc)
		return EXIT_FAILURE;

	switch (action) {
	case START:
		if ((ctrlfd.fd = openat(rc_dirfd(RC_DIR_DAEMONS), "supervise-daemon", O_WRONLY)) == -1)
			return EXIT_FAILURE;

		rc_service_value_fmt(svcname, "argc", "%d", argc);
		argv_stream = xopen_memstream(&cmdline, &len);
		for (int i = 0; i < argc; i++)
			fprintf(argv_stream, "%s\n", argv[i]);
		xclose_memstream(argv_stream);
		rc_service_value_set(svcname, "argv", cmdline);

		if ((env = getenv("SSD_NICELEVEL")))
			rc_service_value_set(svcname, "nicelevel", env);
		if ((env = getenv("SSD_IONICELEVEL")))
			rc_service_value_set(svcname, "ionice", env);
		if ((env = getenv("SSD_OOM_SCORE_ADJ")))
			rc_service_value_set(svcname, "oom-score-adj", env);

		for (size_t i = 0; i < ARRAY_SIZE(longopts); i++)
			rc_service_value_set(svcname, longopts[i].name, start_opts[i]);

		if (write(ctrlfd.fd, svcname, strlen(svcname)) == -1)
			return EXIT_FAILURE;

		do {
			poll(&ctrlfd, 1, -1);
		} while (!(ctrlfd.revents & POLLHUP));

		return EXIT_SUCCESS;
	case STOP:
		if ((ctrlfd.fd = openat(rc_dirfd(RC_DIR_DAEMONS), svcname, O_WRONLY)) == -1)
			return EXIT_FAILURE;
		if (write(ctrlfd.fd, "stop", strlen("stop")) == -1)
			return EXIT_FAILURE;
		break;
	case SIGNAL:
		if ((ctrlfd.fd = openat(rc_dirfd(RC_DIR_DAEMONS), svcname, O_WRONLY)) == -1)
			return EXIT_FAILURE;
		if (argc <= 0)
			return EXIT_FAILURE;
		if (dprintf(ctrlfd.fd, "signal %s", argv[0]) == -1)
			return EXIT_FAILURE;
	}
}
