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
	{ "quiet",          0, NULL, 'q'},

#define longopts_help_COMMON \
	"Display this help output (duh)", \
	"Disable color output", \
	"Run quietly"

#define case_RC_COMMON_GETOPT \
	case 'C': setenv ("RC_NOCOLOR", "yes", 1); break; \
	case 'h': usage (EXIT_SUCCESS); \
	case 'q': setenv ("RC_QUIET", "yes", 1); break; \
	default:  usage (EXIT_FAILURE);
