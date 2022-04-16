#!/bin/sh

top_srcdir=${SOURCE_ROOT:-..}
. ${top_srcdir}/test/setup_env.sh

ebegin "Checking for x* func usage"
out=$(cd ${top_srcdir}; find src -name '*.[ch]' \
	! -name queue.h \
	-exec grep -n -E '\<(malloc|strdup)[[:space:]]*\(' {} + \
	| grep -v \
		-e src/shared/helpers.h \
		-e src/libeinfo/libeinfo.c)

[ -z "${out}" ]
eend $? "These need to be using the x* variant:"$'\n'"${out}"
