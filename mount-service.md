# New Mount Service

This is definitely considered testing code, so if you use it on your
system and it breaks, you have been warned.

This document will evolve as the testing proceeds and will be removed
once the script goes mainline.

Since this is on a topic branch, this branch will be subject to
rebasing, merging commits, etc. When it goes to master, everything here
will be in one commit.

The goal of this is to replace the localmount and metmount services with
a single mount service which will be multiplexed so that there is one
service for each file system that will be mounted.

- rc-service mount generate

scans your fstab and attempts to generate symbolic links in /etc/init.d
for each file system you want to mount.

Then you must edit /etc/conf.d/mount to list all of your file systems
and the dependencies they have by using
rc_mount_foo_before/after/need/use settings.

rc-service mount.foo start

tries to mount a file system and

rc-service mount.foo stop

tries to unmount it if your system is going down.

I'm considering automating the generation of symlinks into the boot
process, but I'm not sure yet whether this is a good idea.

I haven't decided yet exactly how I want to handle the transition from
localmount/netmount to this new mount service yet, so you will
definitely not have a clean boot if you try to boot with this.

Another question is whether I want to try automating adding the mount
services to runlevels; I think no, but let me know if you feel
differently.

Any suggestions would be helpful.

