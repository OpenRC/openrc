/*
   strlist.h 
   String list macros for making char ** arrays
   Copyright 2007 Gentoo Foundation
   Based on a previous implementation by Martin Schlemmer
   Released under the GPLv2
   */

#ifndef __STRLIST_H__
#define __STRLIST_H__

/* FIXME: We should replace the macro with an rc_strlist_foreach
   function, but I'm unsure how to go about this. */

/* Step through each entry in the string list, setting '_pos' to the
   beginning of the entry.  '_counter' is used by the macro as index,
   but should not be used by code as index (or if really needed, then
   it should usually by +1 from what you expect, and should only be
   used in the scope of the macro) */
#define STRLIST_FOREACH(_list, _pos, _counter) \
 if ((_list) && _list[0] && ! (_counter = 0)) \
   while ((_pos = _list[_counter++]))

#endif /* __STRLIST_H__ */
