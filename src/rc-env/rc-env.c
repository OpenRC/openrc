#include <rc.h>
#include <einfo.h>

#include "helpers.h"
#include "queue.h"
#include "_usage.h"

const char *applet = NULL;
const char *extraopts = NULL;
const char *usagestring = "Usage: rc-env [options] [<NAME=VALUE>...]\n";
const char getoptstring[] = "s:u" getoptstring_COMMON;
const struct option longopts[] = {
	{ "service",  required_argument, NULL, 's' },
	{ "unset",    no_argument,       NULL, 'u' },
	longopts_COMMON
};
const char * const longopts_help[] = {
	"Service whose variables to print",
	"Unset listed varibles",
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
	puts("\'\n");
}

static void
print_environment(const char *service)
{
	/* from POSIX.2024 Shell Command Language 2.2 */
	static const char special_chars[] = "|&;<>()$`\\\"' \t\n*?[]^-!#~=%{},";
	struct rc_environ env;

	if (!rc_environ_open(&env, service))
		return;

	for (const char *name, *value; rc_environ_get(&env, &name, &value);) {
		if (strpbrk(value, special_chars))
			escape_variable(value);
		else
			puts(value);
	}

	rc_environ_close(&env);
}

int main(int argc, char **argv) {
	RC_STRINGLIST *services = NULL;
	RC_STRING *service;
	bool unset = false;
	int opt;

	applet = basename_c(argv[0]);
	while ((opt = getopt_long(argc, argv, getoptstring, longopts, NULL)) != -1) {
		switch (opt) {
		case 'u': unset = true; break;
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

	if (!argc) {
		if (!services) {
			services = rc_services_in_state(RC_SERVICE_STARTED);
			print_environment("rc");
		}
		TAILQ_FOREACH(service, services, entries)
			print_environment(service->value);
		rc_stringlist_free(services);
	} else for (int i = 0; i < argc; i++) {
		char *value = argv[i], *name = strsep(&value, "=");

		if (unset && value)
			eerror("invalid variable name '%s=%s'", name, value);
		else if (!unset && !value && !(value = getenv(name)))
			ewarn("no value found for variable '%s'", name);
		else
			rc_service_setenv(NULL, name, value);
	}
}
