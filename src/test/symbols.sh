#!/bin/sh

top_srcdir=${top_srcdir:-../..}
srcdir=${builddir:-..}
top_builddir=${top_srcdir:-../..}
builddir=${builddir:-..}

export LD_LIBRARY_PATH=${builddir}:${LD_LIBRARY_PATH}
. ${top_srcdir}/sh/functions.sh
export PATH=${builddir}:${PATH}

checkit() {
	local base=$1 ; shift
	echo "$@" | tr ' ' '\n' > ${base}.out
	diff -u ${base}.list ${base}.out
	eend $?
	ret=$(($ret + $?))
}

ret=0

ebegin "Checking exported symbols in libeinfo.so (data)"
checkit einfo.data $(
readelf -Ws ${builddir}/libeinfo.so \
	| awk '$4 == "OBJECT" && $5 == "GLOBAL" && $7 != "UND" {print $NF}' \
	| sort -u
)

ebegin "Checking exported symbols in libeinfo.so (functions)"
checkit einfo.funcs $(
readelf -Ws ${builddir}/libeinfo.so \
	| awk '$4 == "FUNC" && $5 == "GLOBAL" && $7 != "UND" {print $NF}' \
	| sort -u \
	| egrep -v \
		-e '^_(init|fini)$'
)

ebegin "Checking exported symbols in librc.so (data)"
checkit rc.data $(
readelf -Ws ${builddir}/librc.so \
	| awk '$4 == "OBJECT" && $5 == "GLOBAL" && $7 != "UND" {print $NF}' \
	| sort -u
)

ebegin "Checking exported symbols in librc.so (functions)"
checkit rc.funcs $(
readelf -Ws ${builddir}/librc.so \
	| awk '$4 == "FUNC" && $5 == "GLOBAL" && $7 != "UND" {print $NF}' \
	| sort -u \
	| egrep -v \
		-e '^_(init|fini)$'
)

ebegin "Checking hidden functions in librc.so"
sed -n '/^librc_hidden_proto/s:.*(\(.*\))$:\1:p' ../librc.h \
	| sort -u \
	> librc.funcs.hidden.list
readelf -Wr $(grep -l '#include[[:space:]]"librc\.h"' ${builddir}/*.c | sed 's:\.c$:.o:') \
	| awk '$5 ~ /^rc_/ {print $5}' \
	| sort -u \
	| egrep -v '^rc_environ_fd$' \
	> librc.funcs.hidden.out
syms=$(diff -u librc.funcs.hidden.list librc.funcs.hidden.out | sed -n '/^+[^+]/s:^+::p')
[ -z "${syms}" ]
eend $? "Missing hidden defs:"$'\n'"${syms}"
ret=$(($ret + $?))

exit ${ret}
