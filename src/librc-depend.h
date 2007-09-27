/*
 * librc-depend.h
 * Internal header file for dependency structures
 * Copyright 2007 Gentoo Foundation
 * Released under the GPLv2
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
