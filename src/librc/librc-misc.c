/*
 * rc-misc.c
 * rc misc functions
*/

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

#include <fnmatch.h>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>

#include "queue.h"
#include "librc.h"
#include "helpers.h"
#include "misc.h"

bool
rc_yesno(const char *value)
{
	if (!value) {
		errno = ENOENT;
		return false;
	}

	if (strcasecmp(value, "yes") == 0 ||
	    strcasecmp(value, "y") == 0 ||
	    strcasecmp(value, "true") == 0 ||
	    strcasecmp(value, "1") == 0)
		return true;

	if (strcasecmp(value, "no") != 0 &&
	    strcasecmp(value, "n") != 0 &&
	    strcasecmp(value, "false") != 0 &&
	    strcasecmp(value, "0") != 0)
		errno = EINVAL;

	return false;
}


/**
 * Read the entire @file into the buffer and set @len to the
 * size of the buffer when finished. For C strings, this will
 * be strlen(buffer) + 1.
 * Don't forget to free the buffer afterwards!
 */
bool
rc_getfile(const char *file, char **buffer, size_t *len)
{
	bool ret = false;
	FILE *fp;
	int fd;
	struct stat st;
	size_t done, left;

	fp = fopen(file, "re");
	if (!fp)
		return false;

	/* assume fileno() never fails */
	fd = fileno(fp);

	if (fstat(fd, &st))
		goto finished;

	left = st.st_size;
	*len = left + 1; /* NUL terminator */
	*buffer = xrealloc(*buffer, *len);
	while (left) {
		done = fread(*buffer, sizeof(*buffer[0]), left, fp);
		if (done == 0 && ferror(fp))
			goto finished;
		left -= done;
	}
	ret = true;

 finished:
	if (!ret) {
		free(*buffer);
		*len = 0;
	} else
		(*buffer)[*len - 1] = '\0';
	fclose(fp);
	return ret;
}

char *
rc_proc_getent(const char *ent RC_UNUSED)
{
#ifdef __linux__
	FILE *fp;
	char *proc = NULL, *p, *value = NULL, *save;
	size_t i, len;

	if (!exists("/proc/cmdline"))
		return NULL;

	if (!(fp = fopen("/proc/cmdline", "r")))
		return NULL;

	i = 0;
	if (xgetline(&proc, &i, fp) == -1) {
		free(proc);
		return NULL;
	}
	save = proc;

	len = strlen(ent);
	while ((p = strsep(&save, " "))) {
		if (strncmp(ent, p, len) == 0 && (p[len] == '\0' || p[len] == ' ' || p[len] == '=')) {
			p += len;
			if (*p == '=')
				p++;
			value = xstrdup(p);
		}
	}

	if (!value)
		errno = ENOENT;

	fclose(fp);
	free(proc);

	return value;
#else
	return NULL;
#endif
}

RC_STRINGLIST *
config_list(int dirfd, const char *pathname)
{
	FILE *fp;
	char *buffer = NULL;
	size_t len = 0;
	char *p;
	char *token;
	RC_STRINGLIST *list = rc_stringlist_new();

	if (!(fp = do_fopenat(dirfd, pathname, O_RDONLY)))
		return list;

	while (xgetline(&buffer, &len, fp) != -1) {
		p = buffer;
		/* Strip leading spaces/tabs */
		while ((*p == ' ') || (*p == '\t'))
			p++;

		/* Get entry - we do not want comments */
		token = strsep(&p, "#");
		if (token && (strlen(token) > 1)) {
			/* If not variable assignment then skip */
			if (strchr(token, '=')) {
				/* Strip the newline if present */
				if (token[strlen(token) - 1] == '\n')
					token[strlen(token) - 1] = 0;

				rc_stringlist_add(list, token);
			}
		}
	}
	free(buffer);
	fclose(fp);

	return list;
}

RC_STRINGLIST *
rc_config_list(const char *file)
{
	return config_list(AT_FDCWD, file);
}

static void rc_config_set_value(RC_STRINGLIST *config, char *value)
{
	RC_STRING *cline;
	char *entry;
	size_t i = 0;
	char *newline;
	char *p = value;
	bool replaced;
	char *token;

	if (!p)
		return;
	if (strncmp(p, "export ", 7) == 0)
		p += 7;
	if (!(token = strsep(&p, "=")))
		return;

	entry = xstrdup(token);
	/* Preserve shell coloring */
	if (*p == '$')
		token = value;
	else
		do {
			/* Bash variables are usually quoted */
			token = strsep(&p, "\"\'");
		} while (token && *token == '\0');

	/* Drop a newline if that's all we have */
	if (token) {
		i = strlen(token) - 1;
		if (token[i] == '\n')
			token[i] = 0;

		xasprintf(&newline, "%s=%s", entry, token);
	} else {
		xasprintf(&newline, "%s=", entry);
	}

	replaced = false;
	/* In shells the last item takes precedence, so we need to remove
	   any prior values we may already have */
	TAILQ_FOREACH(cline, config, entries) {
		i = strlen(entry);
		if (strncmp(entry, cline->value, i) == 0 && cline->value[i] == '=') {
			/* We have a match now - to save time we directly replace it */
			free(cline->value);
			cline->value = newline;
			replaced = true;
			break;
		}
	}

	if (!replaced) {
		rc_stringlist_add(config, newline);
		free(newline);
	}
	free(entry);
}

/*
 * Override some specific rc.conf options on the kernel command line.
 * I only know how to do this in Linux, so if someone wants to supply
 * a patch for this on *BSD or tell me how to write the code to do this,
 * any suggestions are welcome.
 */
static RC_STRINGLIST *rc_config_kcl(RC_STRINGLIST *config)
{
#ifdef __linux__
	RC_STRINGLIST *overrides;
	RC_STRING *cline, *override, *config_np;
	char *tmp = NULL;
	char *value = NULL;
	size_t varlen = 0;

	overrides = rc_stringlist_new();

	/* A list of variables which may be overridden on the kernel command line */
	rc_stringlist_add(overrides, "rc_interactive");
	rc_stringlist_add(overrides, "rc_parallel");

	TAILQ_FOREACH(override, overrides, entries) {
		varlen = strlen(override->value);
		value = rc_proc_getent(override->value);

		/* No need to continue if there's nothing to override */
		if (!value) {
			free(value);
			continue;
		}

		if (value != NULL) {
			xasprintf(&tmp, "%s=%s", override->value, value);
		}

		/*
		 * Whenever necessary remove the old config entry first to prevent
		 * duplicates
		 */
		TAILQ_FOREACH_SAFE(cline, config, entries, config_np) {
			if (strncmp(override->value, cline->value, varlen) == 0
				&& cline->value[varlen] == '=') {
				rc_stringlist_delete(config, cline->value);
				break;
			}
		}

		/* Add the option (var/value) to the current config */
		rc_stringlist_add(config, tmp);

		free(tmp);
		free(value);
	}

	rc_stringlist_free(overrides);
#endif
	return config;
}

static void
rc_config_directory(RC_STRINGLIST *config, int targetfd, const char *dir)
{
	DIR *dp;
	struct dirent *d;
	RC_STRINGLIST *rc_conf_d_files;
	RC_STRING *fname;
	RC_STRINGLIST *rc_conf_d_list;
	RC_STRING *line;

	if (!(dp = do_opendirat(targetfd, dir)))
		return;

	rc_conf_d_files = rc_stringlist_new();
	while ((d = readdir(dp)) != NULL) {
		if (fnmatch("*.conf", d->d_name, FNM_PATHNAME) == 0) {
			rc_stringlist_addu(rc_conf_d_files, d->d_name);
		}
	}

	rc_stringlist_sort(&rc_conf_d_files);
	TAILQ_FOREACH(fname, rc_conf_d_files, entries) {
		if (!fname->value)
			continue;

		rc_conf_d_list = config_list(dirfd(dp), fname->value);
		TAILQ_FOREACH(line, rc_conf_d_list, entries)
			if (line->value)
				rc_config_set_value(config, line->value);
		rc_stringlist_free(rc_conf_d_list);
	}

	rc_stringlist_free(rc_conf_d_files);
	closedir(dp);

	return;
}

static RC_STRINGLIST *
config_load(int dirfd, const char *pathname)
{
	RC_STRINGLIST *list;
	RC_STRINGLIST *config;
	RC_STRING *line;

	list = config_list(dirfd, pathname);
	config = rc_stringlist_new();
	TAILQ_FOREACH(line, list, entries) {
		rc_config_set_value(config, line->value);
	}
	rc_stringlist_free(list);

	return config;
}

RC_STRINGLIST *
rc_config_load(const char *file)
{
	return config_load(AT_FDCWD, file);
}

char *
rc_config_value(RC_STRINGLIST *list, const char *entry)
{
	RC_STRING *line;
	char *p;
	size_t len;

	len = strlen(entry);
	TAILQ_FOREACH(line, list, entries) {
		p = strchr(line->value, '=');
		if (p != NULL) {
			if (strncmp(entry, line->value, len) == 0 && line->value[len] == '=')
				return ++p;
		}
	}
	return NULL;
}

/* Global for caching the strings loaded from rc.conf to avoid reparsing for
 * each rc_conf_value call */
static RC_STRINGLIST *rc_conf = NULL;

static void
_free_rc_conf(void)
{
	rc_stringlist_free(rc_conf);
}

static void
rc_conf_append(enum rc_dir dir)
{
	RC_STRINGLIST *conf = config_load(rc_dirfd(dir), "rc.conf");
	TAILQ_CONCAT(rc_conf, conf, entries);
	rc_stringlist_free(conf);
}

char *
rc_conf_value(const char *setting)
{
	RC_STRING *s;

	if (rc_conf)
		return rc_config_value(rc_conf, setting);

	rc_conf = rc_stringlist_new();
	atexit(_free_rc_conf);

	/* Load user configurations first, as they should override
	 * system wide configs. */
	if (rc_is_user()) {
		rc_conf_append(RC_DIR_USRCONF);
		rc_config_directory(rc_conf, rc_dirfd(RC_DIR_USRCONF), "rc.conf.d");
	}

	rc_conf_append(RC_DIR_SYSCONF);

	/* Support old configs. */
	if (exists(RC_CONF_OLD)) {
		RC_STRINGLIST *old_conf = config_load(AT_FDCWD, RC_CONF_OLD);
		TAILQ_CONCAT(rc_conf, old_conf, entries);
		rc_stringlist_free(old_conf);
	}

	rc_config_directory(rc_conf, rc_dirfd(RC_DIR_SYSCONF), "rc.conf.d");

	rc_conf = rc_config_kcl(rc_conf);

	/* Convert old uppercase to lowercase */
	TAILQ_FOREACH(s, rc_conf, entries) {
		char *p = s->value;
		while (p && *p && *p != '=') {
			if (isupper((unsigned char)*p))
				*p = tolower((unsigned char)*p);
			p++;
		}
	}

	return rc_config_value(rc_conf, setting);
}
