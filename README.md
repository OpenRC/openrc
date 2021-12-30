OpenRC README
=============

OpenRC is a dependency-based init system that works with the
system-provided init program, normally `/sbin/init`.

## building and installing

OpenRC uses the  [meson](http://mesonbuild.com) build system, so use the
usual methods for this build system to build and install.

## Notes

We don't support building a static OpenRC with PAM.

`PKG_PREFIX` should be set to where packages install to by default.

`LOCAL_PREFIX` should be set to where user maintained packages are.
Only set `LOCAL_PREFIX` if different from `PKG_PREFIX`.

`ROOTPREFIX` should be set when the root path is different from '/'.

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

