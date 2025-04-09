/*
 * Copyright (c) 2025 The OpenRC Authors.
 * See the Authors file at the top-level directory of this distribution and
 * https://github.com/OpenRC/openrc/blob/HEAD/AUTHORS
 *
 * This file is part of OpenRC. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution and at https://github.com/OpenRC/openrc/blob/HEAD/LICENSE
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file.
 */

#include <errno.h>

#include "helpers.h"
#include "timeutils.h"

/* on linux CLOCK_BOOTTIME provides proper POSIX CLOCK_MONOTONIC behavior. */
#if defined(__linux__)
#  undef  CLOCK_MONOTONIC
#  define CLOCK_MONOTONIC CLOCK_BOOTTIME
#endif

static int64_t ts_to_ms(struct timespec tv)
{
	int64_t sec_to_ms = 1000, round_up = 500000, ns_to_ms = 1000000;
	return (tv.tv_sec * sec_to_ms) + ((tv.tv_nsec + round_up) / ns_to_ms);
}

int64_t tm_now(void)
{
	struct timespec tv;
	clock_gettime(CLOCK_MONOTONIC, &tv);
	return ts_to_ms(tv);
}

int64_t tm_sleep(int64_t ms, enum tm_sleep_flags flags)
{
	int ret;
	struct timespec tv, rem;
	tv.tv_sec  = ms / 1000;
	tv.tv_nsec = (ms % 1000) * 1000000l;
	for (;;) {
		ret = clock_nanosleep(CLOCK_MONOTONIC, 0, &tv, &rem);
		if (ret < 0 && (!(flags & TM_NO_EINTR) || errno != EINTR))
			return ts_to_ms(rem);
		if (ret == 0)
			break;
		tv = rem;
	}
	return 0;
}

int64_t parse_duration(const char *duration)
{
	enum delay_unit {
		UNIT_INVALID = -1,
		UNIT_MS   = 1,
		UNIT_SEC  = 1000,
		UNIT_MIN  = UNIT_SEC * 60,
		UNIT_HOUR = UNIT_MIN * 60,
	} unit = UNIT_INVALID;
	struct { const char *ext; enum delay_unit unit; } exts[] = {
		{ "ms", UNIT_MS }, { "miliseconds", UNIT_MS },
		{ "s", UNIT_SEC }, { "sec", UNIT_SEC }, { "seconds", UNIT_SEC }, { "", UNIT_SEC },
		{ "m", UNIT_MIN }, { "min", UNIT_MIN }, { "minutes", UNIT_MIN },
		{ "h", UNIT_HOUR }, { "hour", UNIT_HOUR }
	};
	char *end;
	long val;

	errno = 0;
	val = strtol(duration, &end, 10);
	if (errno == ERANGE || *duration == '\0' || val < 0)
		return -1;
	for (size_t i = 0; i < ARRAY_SIZE(exts); ++i) {
		if (strcmp(end, exts[i].ext) == 0) {
			unit = exts[i].unit;
			break;
		}
	}
	switch (unit) {
	case UNIT_INVALID:
		return -1;
	default:
		if (INT64_MAX/unit < val) /* overflow */
			return -1;
		return (int64_t)val * unit;
	}
}
