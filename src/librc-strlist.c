/*
   librc-strlist.h
   String list functions for using char ** arrays

   Based on a previous implementation by Martin Schlemmer
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

#include "librc.h"

static char *_rc_strlist_add (char ***list, const char *item, bool uniq)
{
	char **newlist;
	char **lst = *list;
	int i = 0;

	if (! item)
		return (NULL);

	while (lst && lst[i]) {
		if (uniq && strcmp (lst[i], item) == 0) {
			errno = EEXIST;
			return (NULL);
		}
		i++;
	}

	newlist = xrealloc (lst, sizeof (char *) * (i + 2));
	newlist[i] = xstrdup (item);
	newlist[i + 1] = NULL;

	*list = newlist;
	return (newlist[i]);
}

char *rc_strlist_add (char ***list, const char *item)
{
	return (_rc_strlist_add (list, item, false));
}
librc_hidden_def(rc_strlist_add)

char *rc_strlist_addu (char ***list, const char *item)
{
	return (_rc_strlist_add (list, item, true));
}
librc_hidden_def(rc_strlist_addu)

static char *_rc_strlist_addsort (char ***list, const char *item,
								  int (*sortfunc) (const char *s1,
												   const char *s2),
								  bool uniq)
{
	char **newlist;
	char **lst = *list;
	int i = 0;
	char *tmp1;
	char *tmp2;
	char *retval;

	if (! item)
		return (NULL);

	while (lst && lst[i])	{
		if (uniq && strcmp (lst[i], item) == 0) {
			errno = EEXIST;
			return (NULL);
		}
		i++;
	}

	newlist = xrealloc (lst, sizeof (char *) * (i + 2));

	if (! i)
		newlist[i] = NULL;
	newlist[i + 1] = NULL;

	i = 0;
	while (newlist[i] && sortfunc (newlist[i], item) < 0)
		i++;

	tmp1 = newlist[i];
	retval = newlist[i] = xstrdup (item);
	do {
		i++;
		tmp2 = newlist[i];
		newlist[i] = tmp1;
		tmp1 = tmp2;
	} while (tmp1);

	*list = newlist;
	return (retval);
}

char *rc_strlist_addsort (char ***list, const char *item)
{
	return (_rc_strlist_addsort (list, item, strcoll, false));
}
librc_hidden_def(rc_strlist_addsort)

char *rc_strlist_addsortc (char ***list, const char *item)
{
	return (_rc_strlist_addsort (list, item, strcmp, false));
}
librc_hidden_def(rc_strlist_addsortc)

char *rc_strlist_addsortu (char ***list, const char *item)
{
	return (_rc_strlist_addsort (list, item, strcmp, true));
}
librc_hidden_def(rc_strlist_addsortu)

bool rc_strlist_delete (char ***list, const char *item)
{
	char **lst = *list;
	int i = 0;

	if (!lst || ! item)
		return (false);

	while (lst[i]) {
		if (strcmp (lst[i], item) == 0) {
			free (lst[i]);
			do {
				lst[i] = lst[i + 1];
				i++;
			} while (lst[i]);
			return (true);
		}
		i++;
	}

	errno = ENOENT;
	return (false);
}
librc_hidden_def(rc_strlist_delete)

char *rc_strlist_join (char ***list1, char **list2)
{
	char **lst1 = *list1;
	char **newlist;
	int i = 0;
	int j = 0;

	if (! list2)
		return (NULL);

	while (lst1 && lst1[i])
		i++;

	while (list2[j])
		j++;

	newlist = xrealloc (lst1, sizeof (char *) * (i + j + 1));

	j = 0;
	while (list2[j]) {
		newlist[i] = list2[j];
		/* Take the item off the 2nd list as it's only a shallow copy */
		list2[j] = NULL;
		i++;
		j++;
	}
	newlist[i] = NULL;

	*list1 = newlist;
	return (newlist[i == 0 ? 0 : i - 1]);
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

	while (list[i])
		free (list[i++]);

	free (list);
}
librc_hidden_def(rc_strlist_free)
