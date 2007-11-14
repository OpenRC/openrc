/* 
 * Copyright 2007 Gentoo Foundation
 * Copyright 2007 Roy Marples
 * All rights reserved

 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#define getoptstring_COMMON "Chqv"

#define longopts_COMMON \
	{ "help",           0, NULL, 'h'}, \
	{ "nocolor",        0, NULL, 'C'}, \
	{ "verbose",        0, NULL, 'v'}, \
	{ "quiet",          0, NULL, 'q'}, \
	{ NULL,             0, NULL,  0 }

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
