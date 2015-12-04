/*
 * Copyright 2007-2015 The OpenRC Authors.
 * See the Authors file at the top-level directory of this distribution and
 * https://github.com/OpenRC/openrc/blob/master/AUTHORS
 *
 * This file is part of OpenRC. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution and at https://github.com/OpenRC/openrc/blob/master/LICENSE
 * This file may not be copied, modified, propagated, or distributed
 *    except according to the terms contained in the LICENSE file.
 */

#include "queue.h"
#include "rc.h"

int checkpath(int, char **);
int fstabinfo(int, char **);
int mountinfo(int, char **);
int openrc_run(int, char **);
int rc_depend(int, char **);
int rc_service(int, char **);
int rc_status(int, char **);
int rc_update(int, char **);
int runscript(int, char **);
int start_stop_daemon(int, char **);
int swclock(int, char **);

void run_applets(int, char **);

/* Handy function so we can wrap einfo around our deptree */
RC_DEPTREE *_rc_deptree_load (int, int *);

/* Test to see if we can see pid 1 or not */
bool _rc_can_find_pids(void);
