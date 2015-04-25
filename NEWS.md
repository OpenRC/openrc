# OpenRC NEWS

This file will contain a list of notable changes for each release. Note
the information in this file is in reverse order.

## OpenRC-0.14

The binfmt service, which registers misc binary formats with the Linux
kernel, has been separated from the procfs service. This service will be
automatically added to the boot runlevel for new Linux installs. When
you upgrade, you will need to use rc-update to add it to your boot
runlevel.

The procfs service no longer automounts the deprecated usbfs and
usbdevfs file systems. Nothing should be using usbdevfs any longer, and
if you still need usbfs it can be added to fstab.

Related to the above change, the procfs service no longer attempts to
modprobe the usbcore module. If your device manager does not load it,
you will need to configure the modules service to do so.

The override order of binfmt.d and tmpfiles.d directories has been
changed to match systemd. Files in /run/binfmt.d and /run/tmpfiles.d
override their /usr/lib counterparts, and files in the /etc counterparts
override both /usr/lib and /run.

## OpenRC-0.13.2

A chroot variable has been added to the service script variables.
This fixes the support for running a service in a chroot.
This is documented in man 8 openrc-run.

The netmount service now mounts nfs file systems.
This change was made to correct a fix for an earlier bug.

## OpenRC-0.13

/sbin/rc was renamed to /sbin/openrc and /sbin/runscript was renamed to
/sbin/openrc-run due to naming conflicts with other software.

Backward compatible symbolic links are currently in place so your
system will keep working if you are using the old names; however, it is
strongly advised that you migrate to the new names because the symbolic
links will be removed in the future.
Warnings have been added to assist with this migration; however, due to the
level of noise they produce, they only appear in verbose mode in this release.

The devfs script now handles the initial mounting and setup of the
/dev directory. If /dev has already been mounted by the kernel or an
initramfs, devfs will remount /dev with the correct mount options
instead of mounting a second /dev over the existing mount point.

It attempts to mount /dev from fstab first if an entry exists there. If
it doesn't it attempts to mount devtmpfs if it is configured in the
kernel. If not, it attempts to mount tmpfs.
If none of these is available, an error message is displayed and static
/dev is assumed.

## OpenRC-0.12

The net.* scripts, originally from Gentoo Linux, have
been removed. If you need these scripts, look for a package called
netifrc, which is maintained by them.
