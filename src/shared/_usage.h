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

#define longopt(short, long, args, help) { #long, args, NULL, #short[0] },
#define longopt_only(short, long, args, help) { #long, args, NULL, short },
#define shortopt_no_argument
#define shortopt_required_argument ":"
#define shortopt_optional_argument "::"
#define shortopt(short, long, args, help) #short shortopt_##args
#define opt_null(short, long, args, help)
#define helpstring(short, long, args, help) #help,

#define common_opts(opt)                                                  \
	opt(h, help, no_argument, "Display this help output")                 \
	opt(C, nocolor, no_argument, "Disable color output")                  \
	opt(V, version, no_argument, "Display software version")              \
	opt(v, verbose, no_argument, "Run verbosely")                         \
	opt(q, quiet, no_argument, "Run quietly (repeat to suppress errors)") \
	opt(U, user, no_argument, "Run in user mode")

#define cmdline_opts(opts)                                                                       \
	const struct option longopts[] = { opts(longopt, longopt_only) common_opts(longopt) };       \
	const char getoptstring[] = opts(shortopt, opt_null) common_opts(shortopt);                  \
	const char * const longopts_help[] = { opts(helpstring, opt_null) common_opts(helpstring) }; \

#define case_RC_COMMON_GETOPT                           \
	case 'C': setenv("EINFO_COLOR", "NO", 1);    break; \
	case 'h': usage(EXIT_SUCCESS);               break; \
	case 'V': if (argc == 2) show_version();     break; \
	case 'v': setenv("EINFO_VERBOSE", "YES", 1); break; \
	case 'q': set_quiet_options();               break; \
	case 'U': rc_set_user();                     break; \
	default:  usage(EXIT_FAILURE);               break;

extern const char *applet;
extern const char *extraopts;
extern const char getoptstring[];
extern const struct option longopts[];
extern const char * const longopts_help[];
extern const char *usagestring;

void set_quiet_options(void);
void show_version(void);
void usage(int exit_status);
