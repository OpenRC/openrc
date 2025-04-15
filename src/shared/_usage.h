/*
 * Copyright (c) 2007-2015 The OpenRC Authors.
 * See the Authors file at the top-level directory of this distribution and
 * https://github.com/OpenRC/openrc/blob/HEAD/AUTHORS
 *
 * This file is part of OpenRC. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution and at https://github.com/OpenRC/openrc/blob/HEAD/LICENSE
 * This file may not be copied, modified, propagated, or distributed
 *    except according to the terms contained in the LICENSE file.
 */

#include <getopt.h>
#include <stdlib.h>
#include "librc.h"

#define getoptstring_DIRS "UR"
#define getoptstring_COMMON "ChqVvU"

#define longopts_DIRS                                     \
	{ "user",           0, NULL, 'U'},				      \
	{ "root",           1, NULL, 'R'},				      \

#define longopts_help_DIRS                             \
	"Run in user mode",                                \
	"Use the specified root path"

#define case_RC_DIRS_GETOPT                                \
	case 'U': rc_set_user();                        break; \
	case 'R': rc_set_root(optarg);                  break; \

#define longopts_COMMON							      \
	{ "help",           0, NULL, 'h'},				      \
	{ "nocolor",        0, NULL, 'C'},				      \
	{ "version",        0, NULL, 'V'},				      \
	{ "verbose",        0, NULL, 'v'},				      \
	{ "quiet",          0, NULL, 'q'},				      \
	{ NULL,             0, NULL,  0 }

#define longopts_help_COMMON						      \
	"Display this help output",					      \
	"Disable color output",						      \
	"Display software version",			              \
	"Run verbosely",						      \
	"Run quietly (repeat to suppress errors)",         \

#define case_RC_COMMON_GETOPT                              \
	case 'C': setenv("EINFO_COLOR", "no", true);    break; \
	case 'h': usage(EXIT_SUCCESS);                  break; \
	case 'V': if (argc == 2) show_version();        break; \
	case 'v': setenv("EINFO_VERBOSE", "yes", true); break; \
	case 'q': set_quiet_options();                  break; \
	default:  usage(EXIT_FAILURE);                  break;

extern const char *applet;
extern const char *extraopts;
extern const char getoptstring[];
extern const struct option longopts[];
extern const char * const longopts_help[];
extern const char *usagestring;

void set_quiet_options(void);
void show_version(void);
void usage(int exit_status);
