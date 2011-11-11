#!/bin/sh

: ${top_srcdir:=..}
. $top_srcdir/test/setup_env.sh

ret=0

tret=0
ebegin "Testing yesno()"
for f in yes YES Yes true TRUE True 1 ; do
	if ! yesno $f; then
		: $(( tret += 1 ))
		echo "!$f!"
	fi
done
for f in no NO No false FALSE False 0 ; do
	if yesno $f; then
		: $(( tret += 1 ))
		echo "!$f!"
	fi
done
eend $tret
: $(( ret += $tret ))

exit $ret
