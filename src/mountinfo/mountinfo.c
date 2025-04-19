/*
 * mountinfo.c
 * Obtains information about mounted filesystems.
 */

/*
 * Copyright 2007-2015 The OpenRC Authors.
 * See the Authors file at the top-level directory of this distribution and
 * https://github.com/OpenRC/openrc/blob/HEAD/AUTHORS
 *
 * This file is part of OpenRC. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution and at https://github.com/OpenRC/openrc/blob/HEAD/LICENSE
 * This file may not be copied, modified, propagated, or distributed
 *    except according to the terms contained in the LICENSE file.
 */

#include <stdlib.h>

#include "libmountinfo.h"
#include "queue.h"
#include "rc.h"

int main(int argc, char **argv)
{
	RC_STRINGLIST *nodes;
	RC_STRING *s;
	int result;

	nodes = find_filtered_mounts(argc, argv);
	result = EXIT_FAILURE;

	TAILQ_FOREACH(s, nodes, entries) {
		if (!rc_yesno(getenv("EINFO_QUIET")))
			printf("%s\n", s->value);
		result = EXIT_SUCCESS;
	}

	return result;
}
