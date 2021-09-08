#!/bin/sh

set -e
set -u

os="$1"
net="$2"
rc_libexecdir="$3"
sysconfdir="$4"
sysvinit="$5"

init_d_dir="${sysconfdir}/init.d"
leveldir="${sysconfdir}/runlevels"
sysinitdir="${leveldir}/sysinit"
bootdir="${leveldir}/boot"
defaultdir="${leveldir}/default"
nonetworkdir="${leveldir}/nonetwork"
shutdowndir="${leveldir}/shutdown"

sysinit=
case "${os}" in
	Linux)
		sysinit="${sysinit} cgroups devfs dmesg sysfs"
		;;
esac

boot="bootmisc fsck hostname localmount loopback root swap sysctl urandom"
if [ "${net}" = yes ]; then
	boot="${boot} network staticroute"
fi
boot_BSD="hostid newsyslog savecore syslogd"

case "${os}" in
	DragonFly)
		boot="${boot} ${boot_BSD}"
		;;
	FreeBSD|GNU-kFreeBSD)
		boot="${boot} ${boot_BSD} adjkerntz dumpon modules syscons"
		;;
	Linux)
		boot="${boot} binfmt hwclock keymaps modules mtab procfs
		save-keymaps save-termencoding termencoding"
		;;
	NetBSD)
		boot="${boot} ${boot_BSD} devdb swap-blk tys wscons"
		;;
esac

default="local netmount"

nonetwork="local"

shutdown="savecache"
case "${os}" in
	Linux)
		shutdown="${shutdown} killprocs mount-ro"
		;;
esac

if ! test -d "${DESTDIR}${sysinitdir}"; then
	install -d "${DESTDIR}${sysinitdir}"
	for x in ${sysinit}; do
		ln -snf "${init_d_dir}/$x" "${DESTDIR}${sysinitdir}/$x"
	done
fi

if ! test -d "${DESTDIR}${bootdir}"; then
	install -d "${DESTDIR}${bootdir}"
	for x in ${boot}; do
		ln -snf "${init_d_dir}/$x" "${DESTDIR}${bootdir}/$x"
	done
fi

if ! test -d "${DESTDIR}${defaultdir}"; then
	install -d "${DESTDIR}${defaultdir}"
	for x in ${default}; do
		ln -snf "${init_d_dir}/$x" "${DESTDIR}${defaultdir}/$x"
	done
fi

if ! test -d "${DESTDIR}${nonetworkdir}"; then
	install -d "${DESTDIR}${nonetworkdir}"
	for x in ${nonetwork}; do
		ln -snf "${init_d_dir}/$x" "${DESTDIR}${nonetworkdir}/$x"
	done
fi

if ! test -d "${DESTDIR}${shutdowndir}"; then
	install -d "${DESTDIR}${shutdowndir}"
	for x in ${shutdown}; do
		ln -snf "${init_d_dir}/$x" "${DESTDIR}${shutdowndir}/$x"
	done
fi
if test "${sysvinit}" = yes && test "${os}" = Linux; then \
	for x in tty1 tty2 tty3 tty4 tty5 tty6; do
		ln -snf "${init_d_dir}/agetty" "${DESTDIR}/${init_d_dir}/agetty.$x"
		ln -snf "${init_d_dir}/agetty.$x" "${DESTDIR}/${defaultdir}/agetty.$x"
	done;
fi

ln -snf "${rc_libexecdir}"/sh/functions.sh "${DESTDIR}/${init_d_dir}"
