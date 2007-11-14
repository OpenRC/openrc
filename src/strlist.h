/*
   strlist.h 
   String list macros for making char ** arrays
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
