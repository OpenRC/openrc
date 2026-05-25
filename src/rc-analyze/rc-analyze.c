/*
 * rc-analyze.c
 * Analyze OpenRC boot performance
 */

/*
 * Copyright (c) 2026 The OpenRC Authors.
 * See the Authors file at the top-level directory of this distribution and
 * https://github.com/OpenRC/openrc/blob/HEAD/AUTHORS
 *
 * This file is part of OpenRC. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution and at https://github.com/OpenRC/openrc/blob/HEAD/LICENSE
 * This file may not be copied, modified, propagated, or distributed
 *    except according to the terms contained in the LICENSE file.
 */

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "_usage.h"
#include "einfo.h"
#include "helpers.h"
#include "misc.h"
#include "queue.h"
#include "rc.h"

#define RC_EVENTLOG_DIR "events"
#define RC_EVENTLOG_SERVICES "events/"
#define RC_EVENTLOG_GLOBAL "events/rc"

const char* applet = NULL;
const char* extraopts = "[blame|critical-chain|time]";

const char getoptstring[] = getoptstring_COMMON;
const struct option longopts[] = {
	longopts_COMMON
};
const char* const longopts_help[] = {
	longopts_help_COMMON
};

const char* usagestring = ""
	"Usage: rc-analyze blame\n"
	"   or: rc-analyze critical-chain [service]\n"
	"   or: rc-analyze time\n"
	"\n"
	"Commands:\n"
	"  blame           Show time taken by each service to start\n"
	"  critical-chain  Show critical chain of service dependencies\n"
	"  time            Show time spent in boot and userspace";

/* Service timing information */
struct service_time {
	char* name;
	int64_t start_time;
	int64_t end_time;
	double duration;
	bool valid;
	TAILQ_ENTRY(service_time)
	entries;
};

TAILQ_HEAD(service_time_list, service_time);

/*
 * Parse a millisecond timestamp from eventlog.
 * Format: integer milliseconds since boot
 */
static bool parse_timestamp(const char* str, int64_t* ts)
{
	char* end;
	long long val;

	errno = 0;
	val = strtoll(str, &end, 10);
	if (errno != 0 || *end != '\0' || end == str)
		return false;

	*ts = (int64_t)val;
	return true;
}

static double ms_to_seconds(int64_t ms)
{
	return (double)ms / 1000.0;
}

/*
 * Read the event log for a service and extract timing information.
 */
static struct service_time* read_service_timing(int dirfd, const char* service)
{
	struct service_time* st;
	FILE* fp;
	char* line = NULL;
	size_t len = 0;
	ssize_t nread;
	int fd;
	bool got_starting = false;
	bool got_started = false;

	st = xmalloc(sizeof(*st));
	memset(st, 0, sizeof(*st));
	st->name = xstrdup(service);
	st->valid = false;

	fd = openat(dirfd, service, O_RDONLY);
	if (fd < 0) {
		free(st->name);
		free(st);
		return NULL;
	}

	fp = fdopen(fd, "r");
	if (!fp) {
		close(fd);
		free(st->name);
		free(st);
		return NULL;
	}

	while ((nread = getline(&line, &len, fp)) != -1) {
		char timestamp[32];
		char state[32];

		if (nread > 0 && line[nread - 1] == '\n')
			line[nread - 1] = '\0';

		if (sscanf(line, "%31s %31s", timestamp, state) != 2)
			continue;

		if (strcmp(state, "starting") == 0) {
			if (parse_timestamp(timestamp, &st->start_time))
				got_starting = true;
		} else if (strcmp(state, "started") == 0) {
			if (parse_timestamp(timestamp, &st->end_time))
				got_started = true;
		}
	}

	free(line);
	fclose(fp);

	if (got_starting && got_started) {
		st->duration = ms_to_seconds(st->end_time - st->start_time);
		st->valid = true;
	}

	return st;
}

static void free_service_time(struct service_time* st)
{
	if (st) {
		free(st->name);
		free(st);
	}
}

static int compare_service_time(const void* a, const void* b)
{
	const struct service_time* const* pa = a;
	const struct service_time* const* pb = b;
	const struct service_time* sa = *pa;
	const struct service_time* sb = *pb;

	if (sa->duration > sb->duration)
		return -1;
	if (sa->duration < sb->duration)
		return 1;
	return 0;
}

static char* format_duration(double seconds)
{
	char *ret;
	if (seconds >= 60.0)
		asprintf(&ret, "%dm %.3fs", (int)(seconds / 60), fmod(seconds, 60.0));
	else if (seconds >= 1.0)
		asprintf(&ret, "%.3fs", seconds);
	else
		asprintf(&ret, "%dms", (int)(seconds * 1000));
	return ret;
}

/*
 * rc-analyze blame
 * Show time taken by each service to start, sorted by duration.
 */
static int do_blame(void)
{
	int svcfd;
	int eventsfd;
	DIR* dp;
	struct dirent* d;
	struct service_time** services = NULL;
	size_t num_services = 0;
	size_t alloc_services = 0;
	size_t i;
	char *duration_str;

	svcfd = rc_dirfd(RC_DIR_SVCDIR);
	if (svcfd < 0) {
		eerror("Cannot access service directory");
		return EXIT_FAILURE;
	}

	eventsfd = openat(svcfd, RC_EVENTLOG_SERVICES, O_RDONLY | O_DIRECTORY);
	if (eventsfd < 0) {
		eerror("Cannot access event logs: %s", strerror(errno));
		eerror("No boot data available. Has the system been booted with eventlog enabled?");
		return EXIT_FAILURE;
	}

	dp = fdopendir(eventsfd);
	if (!dp) {
		close(eventsfd);
		eerror("Cannot read event logs: %s", strerror(errno));
		return EXIT_FAILURE;
	}

	/* Read timing information for all services */
	while ((d = readdir(dp)) != NULL) {
		struct service_time* st;

		if (d->d_name[0] == '.')
			continue;

		st = read_service_timing(dirfd(dp), d->d_name);
		if (!st || !st->valid) {
			free_service_time(st);
			continue;
		}

		if (num_services >= alloc_services) {
			alloc_services = alloc_services ? alloc_services * 2 : 32;
			services = xrealloc(services, alloc_services * sizeof(*services));
		}

		services[num_services++] = st;
	}

	closedir(dp);

	if (num_services == 0) {
		printf("No service timing data available.\n");
		return EXIT_SUCCESS;
	}

	/* Sort by duration (longest first) */
	qsort(services, num_services, sizeof(*services), compare_service_time);

	for (i = 0; i < num_services; i++) {
		duration_str = format_duration(services[i]->duration);
		printf("%10s %s\n", duration_str, services[i]->name);
		free_service_time(services[i]);
		free(duration_str);
	}

	free(services);
	return EXIT_SUCCESS;
}

static struct service_time* find_service_time(struct service_time_list* list, const char* name)
{
	struct service_time* st;

	TAILQ_FOREACH(st, list, entries)
	{
		if (strcmp(st->name, name) == 0)
			return st;
	}
	return NULL;
}

/*
 * Print a service in the critical chain tree format.
 * depth=0 is the root (first dependency), increasing depth goes toward target.
 */
static void print_chain_entry(const char* name, struct service_time_list* all_services, int64_t earliest, int depth)
{
	struct service_time* st;
	char *duration_str;
	int i;

	st = find_service_time(all_services, name);

	/* Print indentation with tree characters */
	for (i = 0; i < depth; i++) {
		if (i == depth - 1)
			printf("└─");
		else
			printf("  ");
	}

	/* Print service info */
	if (st && st->valid) {
		double active_at = ms_to_seconds(st->end_time - earliest);
		duration_str = format_duration(st->duration);
		printf("%s @%.3fs +%s\n", name, active_at, duration_str);
		free(duration_str);
	} else {
		printf("%s\n", name);
	}
}

/*
 * rc-analyze critical-chain [service]
 * Show the critical chain of dependencies for a service in tree format.
 * The tree starts with root dependencies at the top and ends with the target.
 */
static int do_critical_chain(const char* target_service)
{
	int svcfd;
	int eventsfd;
	DIR* dp;
	struct dirent* d;
	struct service_time_list all_services;
	struct service_time *st, *st_tmp;
	RC_DEPTREE* deptree;
	RC_STRINGLIST* types;
	RC_STRINGLIST* services;
	RC_STRINGLIST* depends;
	RC_STRING* s;
	char* runlevel = NULL;
	int64_t earliest_start = 0;
	bool has_earliest = false;
	size_t dep_count, i;
	char** dep_array = NULL;

	TAILQ_INIT(&all_services);

	svcfd = rc_dirfd(RC_DIR_SVCDIR);
	if (svcfd < 0) {
		eerror("Cannot access service directory");
		return EXIT_FAILURE;
	}

	eventsfd = openat(svcfd, RC_EVENTLOG_SERVICES, O_RDONLY | O_DIRECTORY);
	if (eventsfd < 0) {
		eerror("Cannot access event logs: %s", strerror(errno));
		return EXIT_FAILURE;
	}

	dp = fdopendir(eventsfd);
	if (!dp) {
		close(eventsfd);
		eerror("Cannot read event logs");
		return EXIT_FAILURE;
	}

	/* Read timing information for all services */
	while ((d = readdir(dp)) != NULL) {
		if (d->d_name[0] == '.')
			continue;

		st = read_service_timing(dirfd(dp), d->d_name);
		if (st) {
			TAILQ_INSERT_TAIL(&all_services, st, entries);
			if (st->valid) {
				if (!has_earliest || st->start_time < earliest_start) {
					earliest_start = st->start_time;
					has_earliest = true;
				}
			}
		}
	}

	closedir(dp);

	/* Load dependency tree */
	deptree = _rc_deptree_load(0, NULL);
	if (!deptree) {
		eerror("Failed to load dependency tree");
		goto cleanup;
	}

	runlevel = rc_runlevel_get();

	types = rc_stringlist_new();
	rc_stringlist_add(types, "ineed");
	rc_stringlist_add(types, "iuse");
	rc_stringlist_add(types, "iafter");

	printf("The time when unit became active is printed after '@'.\n");
	printf("The time the unit took to start is printed after '+'.\n\n");

	if (!target_service) {
		/* Find the service that finished last */
		int64_t max_end = 0;
		struct service_time* last_service = NULL;

		TAILQ_FOREACH(st, &all_services, entries)
		{
			if (st->valid) {
				if (st->end_time > max_end) {
					max_end = st->end_time;
					last_service = st;
				}
			}
		}

		if (!last_service) {
			printf("No timing data available.\n");
			goto done;
		}
		target_service = last_service->name;
	}

	services = rc_stringlist_new();
	rc_stringlist_add(services, target_service);

	depends = rc_deptree_depends(deptree, types, services,
		runlevel, RC_DEP_TRACE | RC_DEP_STRICT | RC_DEP_START);

	dep_count = 0;
	if (depends) {
		TAILQ_FOREACH(s, depends, entries)
		{
			if (strcmp(s->value, target_service) != 0)
				dep_count++;
		}
	}

	if (dep_count > 0) {
		dep_array = xmalloc(dep_count * sizeof(*dep_array));
		i = 0;
		TAILQ_FOREACH(s, depends, entries)
		{
			if (strcmp(s->value, target_service) != 0)
				dep_array[i++] = s->value;
		}
	}

	/* Print dependencies first (from root to leaf), then target. */
	for (i = 0; i < dep_count; i++) {
		print_chain_entry(dep_array[i], &all_services, earliest_start, (int)i);
	}

	/* Print the target service as the final leaf */
	print_chain_entry(target_service, &all_services, earliest_start, (int)dep_count);

	free(dep_array);
	rc_stringlist_free(depends);
	rc_stringlist_free(services);

done:
	rc_stringlist_free(types);
	rc_deptree_free(deptree);
	free(runlevel);

cleanup:
	TAILQ_FOREACH_SAFE(st, &all_services, entries, st_tmp)
	{
		TAILQ_REMOVE(&all_services, st, entries);
		free_service_time(st);
	}

	return EXIT_SUCCESS;
}

/*
 * Get the openrc startup time from the event logs.
 */
static double get_openrc_time(int64_t* openrc_start)
{
	int svcfd;
	int eventsfd;
	DIR* dp;
	struct dirent* d;
	FILE* fp;
	char* line = NULL;
	size_t len = 0;
	ssize_t nread;
	int64_t first_runlevel = 0;
	int64_t last_service = 0;
	bool got_first = false;
	bool got_last = false;

	svcfd = rc_dirfd(RC_DIR_SVCDIR);
	if (svcfd < 0)
		return -1;

	/* Read the global event log for first runlevel event */
	{
		int globalfd = openat(svcfd, RC_EVENTLOG_GLOBAL, O_RDONLY);
		if (globalfd >= 0) {
			fp = fdopen(globalfd, "r");
			if (fp) {
				while ((nread = getline(&line, &len, fp)) != -1) {
					char timestamp[32];
					char event_type[32];

					if (nread > 0 && line[nread - 1] == '\n')
						line[nread - 1] = '\0';

					if (sscanf(line, "%31s %31s", timestamp, event_type) != 2)
						continue;

					if (strcmp(event_type, "runlevel") == 0) {
						if (!got_first) {
							parse_timestamp(timestamp, &first_runlevel);
							got_first = true;
						}
					}
				}
				fclose(fp);
			} else {
				close(globalfd);
			}
		}
	}

	/* Read service logs to find the last service to finish */
	eventsfd = openat(svcfd, RC_EVENTLOG_SERVICES, O_RDONLY | O_DIRECTORY);
	if (eventsfd < 0) {
		free(line);
		return -1;
	}

	dp = fdopendir(eventsfd);
	if (!dp) {
		close(eventsfd);
		free(line);
		return -1;
	}

	while ((d = readdir(dp)) != NULL) {
		int fd;
		int64_t ts;

		if (d->d_name[0] == '.')
			continue;

		fd = openat(dirfd(dp), d->d_name, O_RDONLY);
		if (fd < 0)
			continue;

		fp = fdopen(fd, "r");
		if (!fp) {
			close(fd);
			continue;
		}

		while ((nread = getline(&line, &len, fp)) != -1) {
			char timestamp[32];
			char state[32];

			if (nread > 0 && line[nread - 1] == '\n')
				line[nread - 1] = '\0';

			if (sscanf(line, "%31s %31s", timestamp, state) != 2)
				continue;

			if (strcmp(state, "started") == 0) {
				if (parse_timestamp(timestamp, &ts)) {
					if (!got_last || ts > last_service) {
						last_service = ts;
						got_last = true;
					}
				}
			}
		}

		fclose(fp);
	}

	closedir(dp);
	free(line);

	if (got_first && got_last) {
		if (openrc_start)
			*openrc_start = first_runlevel;
		return ms_to_seconds(last_service - first_runlevel);
	}

	return -1;
}

/*
 * rc-analyze time
 * Show time spent starting services.
 */
static int do_time(void)
{
	double userspace_time;
	char *duration_str;

	userspace_time = get_openrc_time(NULL);

	if (userspace_time > 0) {
		printf("openrc startup finished in ");
		duration_str = format_duration(userspace_time);
		printf("%s\n", duration_str);
	} else {
		printf("no startup available\n");
		return EXIT_FAILURE;
	}

	free(duration_str);
	return EXIT_SUCCESS;
}

int main(int argc, char** argv)
{
	int opt;
	const char* cmd;

	applet = basename_c(argv[0]);

	while ((opt = getopt_long(argc, argv, getoptstring, longopts,
				(int*)0))
		!= -1) {
		switch (opt) {
			case_RC_COMMON_GETOPT
		}
	}

	if (optind >= argc) {
		eerror("No command specified");
		usage(EXIT_FAILURE);
	}

	cmd = argv[optind];

	if (strcmp(cmd, "blame") == 0) {
		return do_blame();
	} else if (strcmp(cmd, "critical-chain") == 0) {
		const char* service = NULL;
		if (optind + 1 < argc)
			service = argv[optind + 1];
		return do_critical_chain(service);
	} else if (strcmp(cmd, "time") == 0) {
		return do_time();
	} else {
		eerror("Unknown command: %s", cmd);
		usage(EXIT_FAILURE);
	}

	return EXIT_SUCCESS;
}
