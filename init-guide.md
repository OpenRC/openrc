# OpenRC init process guide

OpenRC now includes an init process which can be used on Linux systems
in place of sysvinit.

## migrating a live system to openrc-init

Configuring a live system to use this init process is very
straight-forward, but the steps must be completed in this order.

* have your boot loader add "init=/sbin/openrc-init" to the kernel command line

The details of how to do this will vary from distro to distro, so they are
out of scope for this document.

* Install gettys into the runlevels where you need them.

If you are using the provided /etc/init.d/agetty script,, you should
first create symlinks in /etc/init.d to it for the ports where you
want gettys to run, e.g. the following will work if you want gettys on
tty1-tty6.

```
# cd /etc/init.d
# for x in tty1 tty2 tty3 tty4 tty5 tty6; do
  ln -snf agetty agetty.$x
  done
```

Once this is done, use ```rc-update``` as normal to install the agetty
services in the appropriate runlevels.

* Reboot your system.

At this point you are running under openrc-init, and you should use
openrc-shutdown to handle shutting down, powering off, rebooting etc.

## optional sysvinit compatibility

If you build and install OpenRC with MKSYSVINIT=yes, you will build and install
wrappers that make openrc-init compatible with sysvinit -- you will have
commands like "halt" "shutdown" "reboot" and "poweroff".

If you want this functionality on a live system, you should first
migrate the system to openrc-init, remove sysvinit, then rebuild and
install this package with MKSYSVINIT=yes.

package.
migrating your system to openrc-init.

