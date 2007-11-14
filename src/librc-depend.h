/*
 * librc-depend.h
 * Internal header file for dependency structures
 */

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

#ifndef _LIBRC_DEPEND_H
#define _LIBRC_DEPEND_H

/*! @name Dependency structures
 * private to librc - rc.h exposes them just a pointers */

/*! Singly linked list of dependency types that list the services the
 * type is for */
typedef struct rc_deptype
{
	/*! ineed, iuse, iafter, etc */
	char *type;
	/*! NULL terminated list of services */
	char **services;
	/*! Next dependency type */
	struct rc_deptype *next;
} rc_deptype_t;

/*! Singly linked list of services and their dependencies */
typedef struct rc_depinfo
{
	/*! Name of service */
	char *service;
	/*! Dependencies */
	rc_deptype_t *depends;
	/*! Next service dependency type */
	struct rc_depinfo *next;
} rc_depinfo_t;

#endif
