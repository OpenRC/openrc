# Copyright 2007-2008 Roy Marples 

CC ?= gcc
AR ?= ar
RANLIB ?= ranlib
CFLAGS += -O2 -pipe
LDFLAGS += -L.
PICFLAG = -fPIC

# GNU Make way of detecting gcc flags we can use
check_gcc=$(shell if $(CC) $(1) -S -o /dev/null -xc /dev/null >/dev/null 2>&1; \
	  then echo "$(1)"; else echo "$(2)"; fi)

# pmake check for extra cflags 
WEXTRA != for x in -Wdeclaration-after-statement -Wsequence-point -Wextra; do \
	if $(CC) $$x -S -o /dev/null -xc /dev/null >/dev/null 2>&1; \
	then echo -n "$$x "; fi \
	done

# Loads of nice flags to ensure our code is good
CFLAGS += -pedantic -std=c99 \
    -Wall -Wunused -Wimplicit -Wshadow -Wformat=2 \
    -Wmissing-declarations -Wno-missing-prototypes -Wwrite-strings \
    -Wbad-function-cast -Wnested-externs -Wcomment -Winline \
    -Wchar-subscripts -Wcast-align -Wno-format-nonliteral \
    $(call check_gcc, -Wdeclaration-after-statement) \
    $(call check_gcc, -Wsequence-point) \
    $(call check_gcc, -Wextra) $(WEXTRA)

# For debugging. -Werror is pointless due to ISO C issues with dlsym
#CFLAGS += -ggdb

TOPDIR = ..
include $(TOPDIR)/default.mk
