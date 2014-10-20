OpenRC README


Installation
------------
make install
Yup, that simple. Works with GNU make.

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
You may need to use PROGLDFLAGS=-Wl,-Bstatic on glibc instead of just -static.
If you debug memory under valgrind, add -DDEBUG_MEMORY to your CPPFLAGS
so that all malloc memory should be freed at exit.
If you are building OpenRC for a Gentoo Prefix installation, add
MKPREFIX=yes.

You can also brand OpenRC if you so wish like so
BRANDING=\"Gentoo/$(uname -s)\"

PKG_PREFIX should be set to where packages install to by default.
LOCAL_PREFIX should be set when to where user maintained packages are.
Only set LOCAL_PREFIX if different from PKG_PREFIX.
PREFIX should be set when OpenRC is not installed to /.

If any of the following files exist then we do not overwrite them
/etc/devd.conf
/etc/rc
/etc/rc.shutdown
/etc/conf.d/*

rc and rc.shutdown are the hooks from the BSD init into OpenRC.
devd.conf is modified from FreeBSD to call /etc/rc.devd which is a generic
hook into OpenRC.
inittab is the same, but for SysVInit as used by most Linux distributions.
This can be found in the support folder.
Obviously, if you're installing this onto a system that does not use OpenRC
by default then you may wish to backup the above listed files, remove them
and then install so that the OS hooks into OpenRC.

init.d.misc is not installed by default as the scripts will need
tweaking on a per distro basis. They are also non essential to the operation
of the system.

Reporting Bugs
--------------
Since Gentoo Linux is hosting OpenRC development, Bugs should go to
the Gentoo Bugzilla:
	http://bugs.gentoo.org/
They should be filed under the "Gentoo Hosted Projects" product and
the "openrc" component.
