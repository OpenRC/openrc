#!/bin/sh

get_links() {
	sed -e 's/ ,//g' \
		-e '/^\.Sh NAME$/,/\.Sh/ s/\.Nm //p' \
		-n ${MESON_SOURCE_ROOT}/man/${1}
}

MANDIR="$1"
shift
for man in $@; do
	prefix=${man%%.*}
	suffix=${man#*.}
	links=$(get_links ${man})
	for link in ${links}; do
		if [ "${link}" != "${prefix}" ]; then
			ln -sf ${man} ${MESON_INSTALL_DESTDIR_PREFIX}/${MANDIR}/man${suffix}/${link}.${suffix}
		fi
	done
done
