[![Bountysource](https://www.bountysource.com/badge/tracker?tracker_id=256913)](https://www.bountysource.com/trackers/256913-openrc?utm_source=256913&utm_medium=shield&utm_campaign=TRACKER_BADGE)

OpenRC README
=============

OpenRC is a dependency-based init system that works with the system-provided init program, normally `/sbin/init`; however, it is not a replacement for `/sbin/init`.

OpenRC is 100% compatible with Gentoo init scripts, which means one can probably find one for the daemons you want to start in the Gentoo Portage Tree. OpenRC, however, is not exclusively used by Gentoo Linux and can be used on different Linux and BSD systems.

###Installation

    make install

Yup, that simple. Works with GNU make.

Also, OpenRC now available in Debian since Jessie release:

    apt-get install systemd- openrc

Note that OpenRC provides their own implementations of `start-stop-daemon` and `service`. For better experience with OpenRC scripts in Debian, please issue:

    mount --bind /sbin/openrc /sbin/start-stop-daemon
    mount --bind /sbin/openrc /usr/sbin/service

You may wish to tweak the installation with the below arguments

    PROGLDFLAGS=-static
    LIBNAME=lib64
    DESTDIR=/tmp/openrc-image
    MKNET=no
    MKPAM=pam
    MKPREFIX=yes
    MKPKGCONFIG=no
    MKSELINUX=yes
    MKSTATICLIBS=no
    MKTERMCAP=ncurses
    MKTERMCAP=termcap
    MKTOOLS=yes
    PKG_PREFIX=/usr/pkg
    LOCAL_PREFIX=/usr/local
    PREFIX=/usr/local

We don't support building a static OpenRC with PAM.
You may need to use `PROGLDFLAGS=-Wl,-Bstatic` on glibc instead of just `-static`.
If you debug memory under valgrind, add `-DDEBUG_MEMORY` to your `CPPFLAGS` so that all malloc memory should be freed at exit.
If you are building OpenRC for a Gentoo Prefix installation, add `MKPREFIX=yes`.

You can also brand OpenRC if you so wish like so

    BRANDING=\"Gentoo/$(uname -s)\"

`PKG_PREFIX` should be set to where packages install to by default.
`LOCAL_PREFIX` should be set when to where user maintained packages are.
Only set `LOCAL_PREFIX` if different from `PKG_PREFIX`.
PREFIX should be set when OpenRC is not installed to /.

If any of the following files exist then we do not overwrite them

    /etc/devd.conf
    /etc/rc
    /etc/rc.shutdown
    /etc/conf.d/*

`rc` and `rc.shutdown` are the hooks from the BSD init into OpenRC.
`devd.conf` is modified from FreeBSD to call `/etc/rc.devd` which is a generic hook into OpenRC.
`inittab` is the same, but for SysVInit as used by most Linux distributions.
This can be found in the support folder.
Obviously, if you're installing this onto a system that does not use OpenRC by default then you may wish to backup the above listed files, remove them and then install so that the OS hooks into OpenRC.

`init.d.misc` is not installed by default as the scripts will need tweaking on a per distro basis. They are also non essential to the operation of the system.

As of OpenRC-0.12, the net.* scripts, originally from Gentoo Linux, have been removed. If you need these scripts, look for a package called netifrc, which is maintained by them.

As of OpenRC-0.13, two binaries have been renamed due to naming conflicts with other software. The /sbin/rc binary was renamed to `/sbin/openrc`, and `/sbin/runscript` was renamed to `/sbin/openrc-run`.

Backward compatible symbolic links are currently in place so your system will keep working if you are using the old names; however, it is strongly advised that you migrate to the new names because the symbolic links will be removed in the future.

Warnings have been added to assist with this migration; however, they only show in verbose mode in this release due to the level of noise they produce.

Also, the `devfs` script now handles the initial mounting and setup of the `/dev` directory. If `/dev` has already been mounted by the kernel or an initramfs, `devfs` will remount `/dev` with the correct mount options instead of mounting a second `/dev` over the existing mount point.

It attempts to mount `/dev` from fstab first if an entry exists there. If it doesn't it attempts to mount `devtmpfs` if it is configured in the kernel. If not, it attempts to mount `tmpfs`.
If none of these is available, an error message is displayed and static `/dev` is assumed.

###Reporting Bugs

Since Gentoo Linux is hosting OpenRC development, Bugs should go to the Gentoo Bugzilla:
	http://bugs.gentoo.org/
They should be filed under the "Gentoo Hosted Projects" product and the "openrc" component.
