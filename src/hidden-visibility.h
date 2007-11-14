/* 
 * Copyright 2007 Gentoo Foundation
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

#ifndef _HIDDEN_VISIBILITY_H_
#define _HIDDEN_VISIBILITY_H_

#if defined(__ELF__) && defined(__GNUC__)
# define __hidden_asmname(name) __hidden_asmname1 (__USER_LABEL_PREFIX__, name)
# define __hidden_asmname1(prefix, name) __hidden_asmname2(prefix, name)
# define __hidden_asmname2(prefix, name) #prefix name
# define __hidden_proto(name, internal) \
	extern __typeof (name) name __asm__ (__hidden_asmname (#internal)) \
	__attribute__ ((visibility ("hidden")));
# define __hidden_ver(local, internal, name) \
   extern __typeof (name) __EI_##name __asm__(__hidden_asmname (#internal)); \
   extern __typeof (name) __EI_##name __attribute__((alias (__hidden_asmname1 (,#local))))
# define hidden_proto(name) __hidden_proto(name, __RC_##name)
# define hidden_def(name) __hidden_ver(__RC_##name, name, name);
#else
# define hidden_proto(name)
# define hidden_def(name)
#endif

#endif
