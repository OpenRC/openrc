#!/bin/sh

top_srcdir=${SOURCE_ROOT:-..}
. ${top_srcdir}/test/setup_env.sh

ebegin "Checking trailing newlines in code"
out=$(cd ${top_srcdir};
	for f in $(find */ -name '*.[ch]') ; do
		while read -r line; do
			if [ -n "${line}" ]; then
				blankline=
			else
				blankline=1
			fi
		done < "${f}"
		[ -n "${blankline}" ] && printf "%s\n" "${f}"
	done)
[ -z "${out}" ]
eend $? "Trailing newlines need to be deleted:"$'\n'"${out}"
