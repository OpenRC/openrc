#!/bin/sh

top_srcdir=${SOURCE_ROOT:-..}
. ${top_srcdir}/test/setup_env.sh

ebegin "Checking spacing style"
out=$(cd ${top_srcdir}; find src -name '*.[ch]' \
	! -name queue.h \
	-exec grep -n -E \
		-e '\<(for|if|switch|while)\(' \
		-e '\<(for|if|switch|while) \( ' \
		-e ' ;' \
		-e '[[:space:]]$' \
		-e '\){' \
		-e '(^|[^:])//' \
	{} +)
[ -z "${out}" ]
eend $? "These lines violate style rules:"$'\n'"${out}"
