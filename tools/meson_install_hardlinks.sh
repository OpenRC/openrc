#!/bin/sh
set -eu
dir=$1
target=$2
shift 2
for linkname; do
	ln -fn "${DESTDIR-}${dir}/${target}" "${DESTDIR-}${dir}/${linkname}"
done
