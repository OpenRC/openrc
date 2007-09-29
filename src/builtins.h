/*
 * @file builtins.h 
 * @brief declaration for our userland helpers to be built into rc
 * @internal
 *
 * Copyright 2007 Gentoo Foundation
 * Released under the GPLv2
 */

#include "rc.h"

int checkown (int argc, char **argv);
int env_update (int argc, char **argv);
int fstabinfo (int argc, char **argv);
int mountinfo (int argc, char **argv);
int rc_depend (int argc, char **argv);
int rc_status (int argc, char **argv);
int rc_update (int argc, char **argv);
int runscript (int argc, char **argv);
int start_stop_daemon (int argc, char **argv);

/* Handy function so we can wrap einfo around our deptree */
rc_depinfo_t *_rc_deptree_load (void);
