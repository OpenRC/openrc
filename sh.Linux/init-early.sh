#!/bin/sh

# Try and set a font and as early as we can
if [ -e /etc/runlevels/"${RC_DEFAULTLEVEL}"/consolefont \
	 -o -e /etc/runlevels/"${RC_BOOTLEVEL}"/consolefont ] ; then
	termencoding="(K"
	[ -e "${RC_LIBDIR}"/console/unicode ] && termencoding="%G"
	printf "\033%s" "${termencoding}"
	if [ -r "${RC_LIBDIR}"/console/font -a -x /bin/setfont ] ; then
		font="$(cat "${RC_LIBDIR}"/console/font)"
		CONSOLE="${CONSOLE:-/dev/console}"
		[ -c "${CONSOLE}" ] && cons="-C ${CONSOLE}"
		setfont ${cons} "${RC_LIBDIR}"/console/"${font}" 2>/dev/null
	fi
fi

# Ensure we exit 0 so the boot continues
exit 0
