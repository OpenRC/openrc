/*
 * @file builtins.h 
 * @brief declaration for our userland helpers to be built into rc
 * @internal
 *
 * Copyright 2007 Gentoo Foundation
 * Released under the GPLv2
 */

int env_update (int argc, char **argv);
int fstabinfo (int argc, char **argv);
int mountinfo (int argc, char **argv);
int rc_depend (int argc, char **argv);
int rc_status (int argc, char **argv);
int rc_update (int argc, char **argv);
int runscript (int argc, char **argv);
int start_stop_daemon (int argc, char **argv);

