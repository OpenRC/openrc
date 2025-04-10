#include <getopt.h>
#include <poll.h>
#include <stdlib.h>

#include <rc.h>

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
	{ "pidfile",            required_argument, NULL, 'p' },
	{ "retry",              required_argument, NULL, 'R' },

	{ "stdin",              required_argument, NULL, '0' },
	{ "stdout",             required_argument, NULL, '1' },
	{ "stderr",             required_argument, NULL, '2' },
	{ "stdout-logger",      required_argument, NULL, LONGOPT_STDOUT_LOGGER },
	{ "stderr-logger",      required_argument, NULL, LONGOPT_STDERR_LOGGER },

	{ "healthcheck-timer",  required_argument, NULL, 'a'},
	{ "healthcheck-delay",  required_argument, NULL, 'A'},

	{ "respawn-delay",      required_argument, NULL, 'D'},
	{ "respawn-max",        required_argument, NULL, 'm'},
	{ "respawn-period",     required_argument, NULL, 'P'},

	{ "notify",             required_argument, NULL, LONGOPT_NOTIFY},

	{ "user",               required_argument, NULL, 'u' },
	{ "chdir",              required_argument, NULL, 'd' },
	{ "chroot",             required_argument, NULL, 'r' },
	{ "umask",              required_argument, NULL, 'k' },
	{ "nicelevel",          required_argument, NULL, 'N' },
	{ "ionice",             required_argument, NULL, 'I' },
	{ "capabilities",       required_argument, NULL, LONGOPT_CAPABILITIES },
	{ "secbits",            required_argument, NULL, LONGOPT_SECBITS },
	{ "oom-score-adj",      required_argument, NULL, LONGOPT_OOM_SCORE_ADJ },
	{ "no-new-privs",       no_argument,       NULL, LONGOPT_NO_NEW_PRIVS },
	{ "scheduler",          required_argument, NULL, LONGOPT_SCHEDULER },
	{ "scheduler-priority", required_argument, NULL, LONGOPT_SCHEDULER_PRIO },
};

int main(int argc, char **argv) {
	const char *svcname = getenv("RC_SVCNAME"), *command;
	const char *start_opts[ARRAY_SIZE(longopts)] = {0};
	struct pollfd ctrlfd;
	int idx;

	applet = basename_c(argv[0]);

	if (rc_yesno(getenv("RC_USER_SERVICES")))
		rc_set_user();

	while (getopt_long(argc, argv, getoptstring, longopts, &idx) != -1)
		start_opts[idx] = longopts[idx].has_arg ? optarg : "true";

	command = argv[optind++];
	argc -= optind;
	argv += optind;

	if (!argc)
		return EXIT_FAILURE;

	if (strcmp(command, "start") == 0) {
		FILE *argv_stream;
		char *env, *cmdline;
		size_t len;

		/* setup daemon command */
		if (!--argc)
			return EXIT_FAILURE;

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
	}

	if ((ctrlfd.fd = openat(rc_dirfd(RC_DIR_DAEMONS), svcname, O_WRONLY)) == -1)
		return EXIT_FAILURE;

	if (strcmp(command, "stop") == 0) {
		if (write(ctrlfd.fd, "stop", strlen("stop")) == -1)
			return EXIT_FAILURE;
	} else if (strcmp(command, "signal") == 0) {
		if (argc <= 0)
			return EXIT_FAILURE;
		if (dprintf(ctrlfd.fd, "signal %s", argv[0]) == -1)
			return EXIT_FAILURE;
	}
}
