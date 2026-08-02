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
#include <inttypes.h>
#include <stdint.h>
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
 * Get the current monotonic time in milliseconds.
 */
static int64_t tm_now(void)
{
	struct timespec tv;
	int64_t sec_to_ms = 1000, round_up = 500000, ns_to_ms = 1000000;

	clock_gettime(CLOCK_MONOTONIC, &tv);
	return (tv.tv_sec * sec_to_ms) + ((tv.tv_nsec + round_up) / ns_to_ms);
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

	state_name = rc_service_state_name(state);

	fp = do_fopenat(eventsfd, service, O_WRONLY | O_CREAT | O_APPEND);
	close(eventsfd);

	if (!fp)
		return;

	fprintf(fp, "%" PRId64 " %s\n", tm_now(), state_name);
	fclose(fp);
}

/*
 * Log a global system event.
 */
void rc_eventlog_global(const char *event_type, const char *message)
{
	int svcfd;
	FILE *fp;

	if (!event_type || !message)
		return;

	svcfd = rc_dirfd(RC_DIR_SVCDIR);
	if (svcfd < 0)
		return;

	if (rc_eventlog_init() != 0)
		return;

	fp = do_fopenat(svcfd, RC_EVENTLOG_GLOBAL, O_WRONLY | O_CREAT | O_APPEND);
	if (!fp)
		return;

	fprintf(fp, "%" PRId64 " %s %s\n", tm_now(), event_type, message);
	fclose(fp);
}
