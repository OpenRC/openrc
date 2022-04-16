#!/bin/sh

top_srcdir=${SOURCE_ROOT:-..}
. ${top_srcdir}/test/setup_env.sh

ebegin "Checking trailing newlines in code"
out=$(cd ${top_srcdir};
	for f in `find */ -name '*.[ch]'` ; do
		sed -n -e :a -e '/^\n*$/{$q1;N;ba' -e '}' $f || echo $f
	done)
[ -z "${out}" ]
eend $? "Trailing newlines need to be deleted:"$'\n'"${out}"
