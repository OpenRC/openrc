#!/bin/sh

top_srcdir=${top_srcdir:-../..}
. ${top_srcdir}/test/setup_env.sh

libeinfo_srcdir="${srcdir}/../libeinfo"
libeinfo_builddir="${builddir}/../libeinfo"
librc_srcdir="${srcdir}/../librc"
librc_builddir="${builddir}/../librc"
rc_srcdir="${srcdir}/../rc"
rc_builddir="${builddir}/../rc"

checkit() {
	local base=$1; shift
	echo "$@" | tr ' ' '\n' > ${base}.out
	diff -u ${base}.list ${base}.out
	eend $?
	ret=$(($ret + $?))
}

ret=0

ebegin "Checking exported symbols in libeinfo.so (data)"
checkit einfo.data $(
readelf -Ws ${libeinfo_builddir}/libeinfo.so \
	| awk '$4 == "OBJECT" && $5 == "GLOBAL" && $7 != "UND" {print $NF}' \
	| sort -u
)

ebegin "Checking exported symbols in libeinfo.so (functions)"
checkit einfo.funcs $(
readelf -Ws ${libeinfo_builddir}/libeinfo.so \
	| awk '$4 == "FUNC" && $5 == "GLOBAL" && $7 != "UND" {print $NF}' \
	| sort -u \
	| egrep -v \
		-e '^_(init|fini)$'
)

ebegin "Checking exported symbols in librc.so (data)"
checkit rc.data $(
readelf -Ws ${librc_builddir}/librc.so \
	| awk '$4 == "OBJECT" && $5 == "GLOBAL" && $7 != "UND" {print $NF}' \
	| sort -u
)

ebegin "Checking exported symbols in librc.so (functions)"
checkit rc.funcs $(
readelf -Ws ${librc_builddir}/librc.so \
	| awk '$4 == "FUNC" && $5 == "GLOBAL" && $7 != "UND" {print $NF}' \
	| sort -u \
	| egrep -v \
		-e '^_(init|fini)$'
)

ebegin "Checking hidden functions in librc.so"
sed -n '/^librc_hidden_proto/s:.*(\(.*\))$:\1:p' ${librc_srcdir}/librc.h \
	| sort -u \
	> librc.funcs.hidden.list
readelf -Wr $(grep -l '#include[[:space:]]"librc\.h"' ${librc_srcdir}/*.c | sed 's:\.c$:.o:') \
	| awk '$5 ~ /^rc_/ {print $5}' \
	| sort -u \
	| egrep -v '^rc_environ_fd$' \
	> librc.funcs.hidden.out
syms=$(diff -u librc.funcs.hidden.list librc.funcs.hidden.out | sed -n '/^+[^+]/s:^+::p')
[ -z "${syms}" ]
eend $? "Missing hidden defs:"$'\n'"${syms}"
ret=$(($ret + $?))

exit ${ret}
