#ifndef __DAEMON_H__
#define __DAEMON_H__

#include "helpers.h"

RC_NORETURN void supervise(const char *svcname);
RC_NORETURN void child_process(const char *svcname, char **argv);

#endif
