#include <einfo.h>
#include <errno.h>
#include <getopt.h>
#include <rc.h>
#include <queue.h>

#include "_usage.h"
#include "helpers.h"

const char *applet;
const char *extraopts;
const char *usagestring = "rc-environ [-s <service> ...] [-r <runlevel> ...] [-en0]\n";
const char getoptstring[] = "ens:r:0" getoptstring_COMMON;
const struct option longopts[] = {
	{ "export",          no_argument, NULL, 'e' },
	{ "no-escape",       no_argument, NULL, 'n' },
	{ "null",            no_argument, NULL, '0' },
	{ "service",   required_argument, NULL, 's' },
	{ "runlevel",  required_argument, NULL, 'r' },
	longopts_COMMON
};

const char *const longopts_help[] = {
	"Prepend \"export\" to printed variables",
	"Do not perform shell sensitive escape on variable values",
	"End each output line with NUL, not newline",
	"Add service to the list of environments to print",
	"Add services in runlevel to the list of environments to print",
	longopts_help_COMMON
};

static void escape_variable(const char *value) {
	char ch;

	printf("$'");
	for (const char *p = value; *p; p++) {
		switch ((ch = *p)) {
		case '\a': ch = 'a'; break;
		case '\b': ch = 'b'; break;
		case '\f': ch = 'f'; break;
		case '\n': ch = 'n'; break;
		case '\r': ch = 'r'; break;
		case '\t': ch = 't'; break;
		case '\v': ch = 'v'; break;
		/* \e is non-standard. */
		case 0x1b: ch = 'e'; break;
		case '"':
		case '\'':
		case '\\':
			break;
		default:
			putchar(ch);
			continue;
		}
		printf("\\%c", ch);
	}
	printf("\'");
}

static int
print_environment(RC_STRINGLIST *services, bool escape, bool export, char sep)
{
	/* from POSIX.2024 Shell Command Language 2.2, excluding `=` */
	static const char special_chars[] = "|&;<>()$`\\\"' \t\n*?[]^-!#~%{},";
	struct rc_environ env = {0};
	RC_STRING *service;
	const char **envp;
	int ret = 0;

	if (!services)
		services = rc_services_in_state(RC_SERVICE_STARTED);

	TAILQ_FOREACH(service, services, entries) {
		if (!rc_service_getenv(service->value, &env)) {
			ewarn("%s: failed to read environment for '%s': %s", applet, service->value, strerror(errno));
			ret = 1;
		}
	}

	rc_environ_export(&env, NULL, &envp);

	for (size_t i = 0; envp[i]; i++) {
		int name_len = strcspn(envp[i], "=");
		const char *value = &envp[i][name_len + 1];

		printf("%s%.*s=", export ? "export " : "", name_len, envp[i]);

		if (escape && strpbrk(value, special_chars))
			escape_variable(value);
		else
			fputs(value, stdout);
		fputc(sep, stdout);
	}

	free(envp), rc_environ_free(&env);
	return ret;
}

int main(int argc, char **argv) {
	RC_STRINGLIST *services = NULL, *in_runlevel;
	bool export = false, escape = true;
	char sep = '\n';

	for (char opt; (opt = getopt_long(argc, argv, getoptstring, longopts, NULL)) != -1;) {
		switch (opt) {
		case 'e': export = true; break;
		case 'n': escape = false; break;
		case '0': sep = '\0'; break;
		case 'r':
			if (!services)
				services = rc_stringlist_new();
			in_runlevel = rc_services_in_runlevel(optarg);
			TAILQ_CONCAT(services, in_runlevel, entries);
			rc_stringlist_free(in_runlevel);
			break;
		case 's':
			if (!services)
				services = rc_stringlist_new();
			rc_stringlist_addu(services, optarg);
			break;
		case_RC_COMMON_GETOPT
		}
	}

	argc -= optind;
	argv += optind;

	if (argc)
		usage(EXIT_FAILURE);

	return print_environment(services, escape, export, sep);
}
