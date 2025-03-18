#!/bin/sh
# can be used for testing do_unmount:
#    # ./tools/manymounts.sh
#    # do_unmount -- -p '^/tmp/manymounts.*'

set -- "0" "1" "2" "3" "4" "5" "6" "7" "8" "9" "A" "B" "C" "D" "E" "F"

mntdir="/tmp/manymounts"
for a in "$@"; do
	mkdir -p "${mntdir}/${a}"
	mount -t tmpfs -o size=256K tmpfs "${mntdir}/${a}"
	for b in "$@"; do
		mkdir -p "${mntdir}/${a}/${b}"
		mount -t tmpfs -o size=256K tmpfs "${mntdir}/${a}/${b}"
		for c in "$@"; do
			mkdir -p "${mntdir}/${a}/${b}/${c}"
			mount -t tmpfs -o size=256K tmpfs "${mntdir}/${a}/${b}/${c}"
		done
	done
done
