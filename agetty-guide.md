# Setting up the agetty service in OpenRC

The agetty service is an OpenRC specific way to monitor and respawn a
getty, using agetty, on Linux. To use this method, make sure you aren't
spawning a getty manager for this port some other way (such as through
sysvinit/inittab), then run the following commands as root.

Note that [port] refers to the port you are spawning the getty on, and
not the full path to it. For example, tty0 or ttyS0instead of /dev/tty0
or /dev/ttyS0.
tty0 or ttyS0, not the full path to it (for example, tty0 or ttyS0 and

```
# cd /etc/init.d
# ln -s agetty agetty.[port]
# cd /etc/conf.d
# cp agetty agetty.[port]
#rc-update add agetty.[port] [runlevel]
```
