/*
 * @file _usage.h
 * @brief standardize help/usage output across all our programs
 * @internal
 *
 * Copyright 2007 Gentoo Foundation
 * Released under the GPLv2
 */

#define getoptstring_COMMON "Chq"

#define longopts_COMMON \
	{ "help",           0, NULL, 'h'}, \
	{ "nocolor",        0, NULL, 'C'}, \
	{ "verbose",        0, NULL, 'v'}, \
	{ "quiet",          0, NULL, 'q'},

#define longopts_help_COMMON \
	"Display this help output (duh)", \
	"Disable color output", \
	"Run verbosely", \
	"Run quietly"

#define case_RC_COMMON_getopt_case_C  setenv ("RC_NOCOLOR", "yes", 1);
#define case_RC_COMMON_getopt_case_h  usage (EXIT_SUCCESS);
#define case_RC_COMMON_getopt_case_v  setenv ("RC_VERBOSE", "yes", 1);
#define case_RC_COMMON_getopt_case_q  setenv ("RC_QUIET", "yes", 1);
#define case_RC_COMMON_getopt_default usage (EXIT_FAILURE);

#define case_RC_COMMON_GETOPT \
	case 'C': case_RC_COMMON_getopt_case_C; break; \
	case 'h': case_RC_COMMON_getopt_case_h; break; \
	case 'v': case_RC_COMMON_getopt_case_v; break; \
	case 'q': case_RC_COMMON_getopt_case_q; break; \
	default:  case_RC_COMMON_getopt_default; break;
