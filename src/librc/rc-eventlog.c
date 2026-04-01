/*
 * rc-eventlog.c
 * Event logging for OpenRC services and system events
 */

/*
 * Copyright (c) 2024 The OpenRC Authors.
 * See the Authors file at the top-level directory of this distribution and
 * https://github.com/OpenRC/openrc/blob/HEAD/AUTHORS
 *
 * This file is part of OpenRC. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution and at https://github.com/OpenRC/openrc/blob/HEAD/LICENSE
 * This file may not be copied, modified, propagated, or distributed
 *    except according to the terms contained in the LICENSE file.
 */

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "librc.h"

#define RC_EVENTLOG_DIR		"events"
#define RC_EVENTLOG_SERVICES	"events/"
#define RC_EVENTLOG_GLOBAL	"events/rc"

/*
 * Get the name of a service state.
 */
static const char *rc_service_state_name(RC_SERVICE state)
{
	int i;

	for (i = 0; rc_service_state_names[i].name; i++) {
		if (rc_service_state_names[i].state == state)
			return rc_service_state_names[i].name;
	}

	return "unknown";
}

/*
 * Format the current time as an ISO 8601 timestamp with milliseconds.
 * Returns a dynamically allocated string that must be freed by the caller.
 */
static char *format_timestamp(void)
{
	time_t now;
	struct tm *tm;
	char timebuf[32];
	char *result;
	int ms;

	clock_gettime(CLOCK_REALTIME, &ts);
	tm = localtime(&ts.tv_sec);
	ms = (int)(ts.tv_nsec / 1000000);

	strftime(timebuf, sizeof(timebuf), "%Y-%m-%dT%H:%M:%S", tm);
	xasprintf(&result, "%s.%03d%+03ld%02ld", timebuf, ms,
		tm->tm_gmtoff / 3600, (labs(tm->tm_gmtoff) % 3600) / 60);

	return result;
}

/*
 * Initialize the eventlog system.
 * Creates necessary directories under <RC_DIR_SVCDIR>/events/
 */
int rc_eventlog_init(void)
{
	int svcfd = rc_dirfd(RC_DIR_SVCDIR);

	if (svcfd < 0)
		return -1;

	if (mkdirat(svcfd, RC_EVENTLOG_DIR, 0755) == -1 && errno != EEXIST)
		return -1;

	if (mkdirat(svcfd, RC_EVENTLOG_SERVICES, 0755) == -1 && errno != EEXIST)
		return -1;

	return 0;
}

/*
 * Log an event for a specific service.
 */
void rc_eventlog_service(const char *service, RC_SERVICE state)
{
	int svcfd;
	int eventsfd;
	char *timestamp;
	FILE *fp;
	const char *state_name;

	if (!service)
		return;

	svcfd = rc_dirfd(RC_DIR_SVCDIR);
	if (svcfd < 0)
		return;

	if (rc_eventlog_init() != 0)
		return;

	eventsfd = openat(svcfd, RC_EVENTLOG_SERVICES, O_RDONLY | O_DIRECTORY);
	if (eventsfd < 0)
		return;

	service = basename_c(service);

	timestamp = format_timestamp();

	state_name = rc_service_state_name(state);

	fp = do_fopenat(eventsfd, service, O_WRONLY | O_CREAT | O_APPEND);
	close(eventsfd);

	if (!fp) {
		free(timestamp);
		return;
	}

	fprintf(fp, "%s %s\n", timestamp, state_name);
	fclose(fp);
	free(timestamp);
}

/*
 * Log a global system event.
 */
void rc_eventlog_global(const char *event_type, const char *message)
{
	int svcfd;
	char *timestamp;
	FILE *fp;

	if (!event_type || !message)
		return;

	svcfd = rc_dirfd(RC_DIR_SVCDIR);
	if (svcfd < 0)
		return;

	if (rc_eventlog_init() != 0)
		return;

	timestamp = format_timestamp();

	fp = do_fopenat(svcfd, RC_EVENTLOG_GLOBAL, O_WRONLY | O_CREAT | O_APPEND);
	if (!fp) {
		free(timestamp);
		return;
	}

	fprintf(fp, "%s %s %s\n", timestamp, event_type, message);
	fclose(fp);
	free(timestamp);
}
