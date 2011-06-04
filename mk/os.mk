# Copyright (c) 2008 Roy Marples <roy@marples.name>
# All rights reserved. Released under the 2-clause BSD license.

# Generic definitions

_OS_SH=		uname -s
_OS:= 		$(shell ${_OS_SH})
OS?= 		${_OS}
include ${MK}/os-${OS}.mk

RC_LIB=		/$(LIBNAME)/rc

