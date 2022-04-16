#!/bin/sh

top_srcdir=${SOURCE_ROOT:-..}
. ${top_srcdir}/test/setup_env.sh

ebegin "Checking trailing whitespace in code"
# XXX: Should we check man pages too ?
out=$(cd ${top_srcdir}; find */ \
	'(' -name '*.[ch]' -o -name '*.in' -o -name '*.sh' ')' \
	-exec grep -n -E '[[:space:]]+$' {} +)
[ -z "${out}" ]
eend $? "Trailing whitespace needs to be deleted:"$'\n'"${out}"
