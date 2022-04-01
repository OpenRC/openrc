OpenRC README
=============

OpenRC is a dependency-based init system that works with the
system-provided init program, normally `/sbin/init`.

## building and installing

OpenRC uses the  [meson](http://mesonbuild.com) build system, so use the
usual methods for this build system to build and install.

The old build system is still available for the 0.44.x branch, but it is
considered deprecated and will be removed. The previous documentation is
below.

## Installation (historical)

OpenRC requires GNU make.

Once you have GNU Make installed, the default OpenRC installation can be
executed using this command:

`make install`

## Configuration (historical)

You may wish to configure the installation by passing one or more of the
below arguments to the make command

```
PROGLDFLAGS=-static
LIBNAME=lib64
DESTDIR=/tmp/openrc-image
MKBASHCOMP=no
MKGETTYS=6
MKNET=no
MKPAM=pam
MKCAP=yes
MKPREFIX=yes
MKPKGCONFIG=no
MKSELINUX=yes
MKSTATICLIBS=no
MKSYSVINIT=yes
MKTERMCAP=ncurses
MKTERMCAP=termcap
MKZSHCOMP=no
PKG_PREFIX=/usr/pkg
LOCAL_PREFIX=/usr/local
PREFIX=/usr/local
BRANDING=\"Gentoo/$(uname -s)\"
SH=/bin/sh
```

## Notes

We don't support building a static OpenRC with PAM.

You may need to use `PROGLDFLAGS=-Wl,-Bstatic` on glibc instead of just `-static`
(This is now handled by the meson build system).

If you are building OpenRC for a Gentoo Prefix installation, add `MKPREFIX=yes`
(this is not supported in the meson build currently, but patches are welcome).

`PKG_PREFIX` should be set to where packages install to by default.

`LOCAL_PREFIX` should be set to where user maintained packages are.
Only set `LOCAL_PREFIX` if different from `PKG_PREFIX`.

`PREFIX` should be set when OpenRC is not installed to /.

If any of the following files exist then we do not overwrite them

```
/etc/devd.conf
/etc/rc
/etc/rc.shutdown
/etc/conf.d/*
```

`rc` and `rc.shutdown` are the hooks from the BSD init into OpenRC.

`devd.conf` is modified from FreeBSD to call `/etc/rc.devd` which is a
generic hook into OpenRC.

`inittab` is the same, but for SysVInit as used by most Linux distributions.
This can be found in the support folder.

Obviously, if you're installing this onto a system that does not use
OpenRC by default then you may wish to backup the above listed files,
remove them and then install so that the OS hooks into OpenRC.

## Discussions

We are testing [discussions](https://github.com/OpenRC/openrc/discussions), so
feel free to open topics there.

## Reporting Bugs

Please report bugs on our [bug tracker](http://github.com/OpenRC/openrc/issues).

If you can contribute code , please feel free to do so by opening
[pull requests](https://github.com/OpenRC/openrc/pulls).

## IRC Channel

We have an official irc channel, #openrc on the libera network.
Please connect your irc client to irc.libera.chat and join #openrc on
that network.

