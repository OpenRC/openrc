/*
   librc-strlist.h
   String list functions for using char ** arrays

   Copyright 2007 Gentoo Foundation
   Based on a previous implementation by Martin Schlemmer
   Released under the GPLv2
   */

#include "librc.h"

static char **_rc_strlist_add (char **list, const char *item, bool uniq)
{
	char **newlist;
	int i = 0;

	if (! item)
		return (list);

	while (list && list[i]) {
		if (uniq && strcmp (list[i], item) == 0)
			return (list);
		i++;
	}

	newlist = rc_xrealloc (list, sizeof (char *) * (i + 2));
	newlist[i] = rc_xstrdup (item);
	newlist[i + 1] = NULL;

	return (newlist);
}

char **rc_strlist_add (char **list, const char *item)
{
	return (_rc_strlist_add (list, item, false));
}
librc_hidden_def(rc_strlist_add)

char **rc_strlist_addu (char **list, const char *item)
{
	return (_rc_strlist_add (list, item, true));
}
librc_hidden_def(rc_strlist_addu)

static char **_rc_strlist_addsort (char **list, const char *item,
								   int (*sortfunc) (const char *s1,
													const char *s2),
								   bool uniq)
{
	char **newlist;
	int i = 0;
	char *tmp1;
	char *tmp2;

	if (! item)
		return (list);

	while (list && list[i])	{
		if (uniq && strcmp (list[i], item) == 0)
			return (list);
		i++;
	}

	newlist = rc_xrealloc (list, sizeof (char *) * (i + 2));

	if (! i)
		newlist[i] = NULL;
	newlist[i + 1] = NULL;

	i = 0;
	while (newlist[i] && sortfunc (newlist[i], item) < 0)
		i++;

	tmp1 = newlist[i];
	newlist[i] = rc_xstrdup (item);
	do {
		i++;
		tmp2 = newlist[i];
		newlist[i] = tmp1;
		tmp1 = tmp2;
	} while (tmp1);

	return (newlist);
}

char **rc_strlist_addsort (char **list, const char *item)
{
	return (_rc_strlist_addsort (list, item, strcoll, false));
}
librc_hidden_def(rc_strlist_addsort)

char **rc_strlist_addsortc (char **list, const char *item)
{
	return (_rc_strlist_addsort (list, item, strcmp, false));
}
librc_hidden_def(rc_strlist_addsortc)

char **rc_strlist_addsortu (char **list, const char *item)
{
	return (_rc_strlist_addsort (list, item, strcmp, true));
}
librc_hidden_def(rc_strlist_addsortu)

char **rc_strlist_delete (char **list, const char *item)
{
	int i = 0;

	if (!list || ! item)
		return (list);

	while (list[i])
		if (! strcmp (list[i], item)) {
			free (list[i]);
			do {
				list[i] = list[i + 1];
				i++;
			} while (list[i]);
		}

	return (list);
}
librc_hidden_def(rc_strlist_delete)

char **rc_strlist_join (char **this, char **that)
{
	char **newlist;
	int i = 0;
	int j = 0;

	if (! this && that)
		return (that);
	if (! that && this)
		return (this);
	if (! that && ! this)
		return (NULL);

	while (this[i])
		i++;

	while (that[j])
		j++;

	newlist = rc_xrealloc (this, sizeof (char *) * (i + j + 1));

	j = 0;
	while (that[j]) {
		newlist[i] = that[j];
		i++;
		j++;
	}
	newlist[i] = NULL;

	return (newlist);
}
librc_hidden_def(rc_strlist_join)

void rc_strlist_reverse (char **list)
{
	char *item;
	int i = 0;
	int j = 0;

	if (! list)
		return;

	while (list[j])
		j++;
	j--;

	while (i < j && list[i] && list[j]) {
		item = list[i];
		list[i] = list[j];
		list[j] = item;
		i++;
		j--;
	}
}
librc_hidden_def(rc_strlist_reverse)

void rc_strlist_free (char **list)
{
	int i = 0;

	if (! list)
		return;

	while (list[i]) {
		free (list[i]);
		list[i++] = NULL;
	}

	free (list);
}
librc_hidden_def(rc_strlist_free)
