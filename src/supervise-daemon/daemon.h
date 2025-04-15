#ifndef __DAEMON_H__
#define __DAEMON_H__

#include "misc.h"
#include "helpers.h"

RC_NORETURN void supervise(const char *svcname);
RC_NORETURN void child_process(const char *svcname, struct notify *notify, char **argv);

#endif
