/*
  rc-misc.c
  rc misc functions
*/

/*
 * Copyright (c) 2007-2008 Roy Marples <roy@marples.name>
 *
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

#include "queue.h"
#include "librc.h"

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
librc_hidden_def(rc_yesno)


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
librc_hidden_def(rc_getfile)

ssize_t
rc_getline(char **line, size_t *len, FILE *fp)
{
	char *p;
	size_t last = 0;

	while (!feof(fp)) {
		if (*line == NULL || last != 0) {
			*len += BUFSIZ;
			*line = xrealloc(*line, *len);
		}
		p = *line + last;
		memset(p, 0, BUFSIZ);
		if (fgets(p, BUFSIZ, fp) == NULL)
			break;
		last += strlen(p);
		if (last && (*line)[last - 1] == '\n') {
			(*line)[last - 1] = '\0';
			break;
		}
	}
	return last;
}
librc_hidden_def(rc_getline)

char *
rc_proc_getent(const char *ent)
{
#ifdef __linux__
	FILE *fp;
	char *proc, *p, *value = NULL;
	size_t i, len;

	if (!exists("/proc/cmdline"))
		return NULL;

	if (!(fp = fopen("/proc/cmdline", "r")))
		return NULL;

	proc = NULL;
	i = 0;
	if (rc_getline(&proc, &i, fp) == -1 || proc == NULL)
		return NULL;

	if (proc != NULL) {
		len = strlen(ent);

		while ((p = strsep(&proc, " "))) {
			if (strncmp(ent, p, len) == 0 && (p[len] == '\0' || p[len] == ' ' || p[len] == '=')) {
				p += len;

				if (*p == '=')
					p++;

				value = xstrdup(p);
			}
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
librc_hidden_def(rc_proc_getent)

RC_STRINGLIST *
rc_config_list(const char *file)
{
	FILE *fp;
	char *buffer = NULL;
	size_t len = 0;
	char *p;
	char *token;
	RC_STRINGLIST *list = rc_stringlist_new();

	if (!(fp = fopen(file, "r")))
		return list;

	while ((rc_getline(&buffer, &len, fp))) {
		p = buffer;
		/* Strip leading spaces/tabs */
		while ((*p == ' ') || (*p == '\t'))
			p++;

		/* Get entry - we do not want comments */
		token = strsep(&p, "#");
		if (token && (strlen(token) > 1)) {
			/* If not variable assignment then skip */
			if (strchr(token, '=')) {
				/* Stip the newline if present */
				if (token[strlen(token) - 1] == '\n')
					token[strlen(token) - 1] = 0;

				rc_stringlist_add(list, token);
			}
		}
	}
	fclose(fp);
	free(buffer);

	return list;
}
librc_hidden_def(rc_config_list)

/*
 * Override some specific rc.conf options on the kernel command line
 */
#ifdef __linux__
static RC_STRINGLIST *rc_config_override(RC_STRINGLIST *config)
{
	RC_STRINGLIST *overrides;
	RC_STRING *cline, *override, *config_np;
	char *tmp = NULL;
	char *value = NULL;
	size_t varlen = 0;
	size_t len = 0;

	overrides = rc_stringlist_new();

	/* A list of variables which may be overridden on the kernel command line */
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
			len = varlen + strlen(value) + 2;
			tmp = xmalloc(sizeof(char) * len);
			snprintf(tmp, len, "%s=%s", override->value, value);
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
	return config;
}
#endif

RC_STRINGLIST *
rc_config_load(const char *file)
{
	RC_STRINGLIST *list;
	RC_STRINGLIST *config;
	char *token;
	RC_STRING *line;
	RC_STRING *cline;
	size_t i = 0;
	bool replaced;
	char *entry;
	char *newline;
	char *p;

	list = rc_config_list(file);
	config = rc_stringlist_new();
	TAILQ_FOREACH(line, list, entries) {
		/* Get entry */
		p = line->value;
		if (! p)
			continue;
		if (strncmp(p, "export ", 7) == 0)
			p += 7;
		if (! (token = strsep(&p, "=")))
			continue;

		entry = xstrdup(token);
		/* Preserve shell coloring */
		if (*p == '$')
			token = line->value;
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

			i = strlen(entry) + strlen(token) + 2;
			newline = xmalloc(sizeof(char) * i);
			snprintf(newline, i, "%s=%s", entry, token);
		} else {
			i = strlen(entry) + 2;
			newline = xmalloc(sizeof(char) * i);
			snprintf(newline, i, "%s=", entry);
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
	rc_stringlist_free(list);

#ifdef __linux__
	/* Only override rc.conf settings */
	if (strcmp(file, RC_CONF) == 0) {
		config = rc_config_override(config);
	}
#endif

	return config;
}
librc_hidden_def(rc_config_load)

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
librc_hidden_def(rc_config_value)

/* Global for caching the strings loaded from rc.conf to avoid reparsing for
 * each rc_conf_value call */
static RC_STRINGLIST *rc_conf = NULL;

char *
rc_conf_value(const char *setting)
{
	RC_STRINGLIST *old;
	RC_STRING *s;
	char *p;

	if (! rc_conf) {
		rc_conf = rc_config_load(RC_CONF);
#ifdef DEBUG_MEMORY
		atexit(_free_rc_conf);
#endif

		/* Support old configs. */
		if (exists(RC_CONF_OLD)) {
			old = rc_config_load(RC_CONF_OLD);
			TAILQ_CONCAT(rc_conf, old, entries);
#ifdef DEBUG_MEMORY
			free(old);
#endif
		}

		/* Convert old uppercase to lowercase */
		TAILQ_FOREACH(s, rc_conf, entries) {
			p = s->value;
			while (p && *p && *p != '=') {
				if (isupper((unsigned char)*p))
					*p = tolower((unsigned char)*p);
				p++;
			}
		}
	}

	return rc_config_value(rc_conf, setting);
}
librc_hidden_def(rc_conf_value)

#ifdef DEBUG_MEMORY
static void
_free_rc_conf(void)
{
	rc_stringlist_free(rc_conf);
}
#endif
