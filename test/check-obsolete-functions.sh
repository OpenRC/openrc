#!/bin/sh

top_srcdir=${SOURCE_ROOT:-..}
. ${top_srcdir}/test/setup_env.sh

ebegin "Checking for obsolete functions"
out=$(cd ${top_srcdir}; find src -name '*.[ch]' \
	! -name queue.h \
	-exec grep -n -E '\<(malloc|memory|sys/(errno|fcntl|signal|stropts|termios|unistd))\.h\>' {} +)
[ -z "${out}" ]
eend $? "Avoid these obsolete functions:"$'\n'"${out}"
