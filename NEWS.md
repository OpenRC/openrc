OpenRC NEWS
===========

This file will contain a list of notable changes for each release. Note
the information in this file is in reverse order.

## OpenRC 0.62

supervise-daemon and start-stop-daemon now support -1/--stdin as means
of piping a file or fifo to the standard input of the daemon.

openrc-init now checks for reapable children instead of always waiting
3 seconds at shutdown.

the `ready` variable was renamed to `notify` to better match other software.

`notify` now supports `socket` as a general compatibility setting for
systemd's "notify-socket" mechanism. at the moment, `notify=socket:ready`
enables readiness notification with `READY=1`

openrc no longer attempts to store cache in libexec, instead using
`/var/cache/rc` if /var is available at boot

openrc-user now starts the `boot` runlevel before starting `default`, and
pam_openrc will wait for `boot` to finish before continuing with the login
flow, this allows for users to start services that need to be up reliably
early at login.

openrc-init now respects the EINFO_QUIET variable, to allow for quiet boots.

rc-status now has -i/--in-state to allow filtering of service status to a
given state.

openrc now mounts /sys/fs/bpf if available.

fixed issues with openrc-run hanging should it be launched with SIGCHLD masked.

## OpenRC 0.61

This release fixes loading conf.d from user directory and fixes setting
variables expected for a new session (USER, LOGNAME, SHELL).


## OpenRC 0.60

openrc now supports s6 "fd" style readiness notification via the `ready`
variable.

openrc now supports running services in a user session via the --user flag
and an optional pam module

Both of these features are currently experimental, so it is important to
report bugs.

## OpenRC 0.56

openrc-run now respects using SIGUSR1 to skip marking a service stopped/started.

RC_ULIMIT now supports setting multiple limits at once.

## OpenRC 0.55

OpenRC now supports shared mounts in fstab. This is used for some containers
which might require  hierarchies to be mounted with different
propagation than the kernel default of private.


Podman containers now can be autodetected.

The openrc-service-script-completion bash completion file has been
removed since it doesn't work well with modern bash completion which
prefers lazy loading.

## OpenRC 0.54

This release drops the support for the split-usr build option.
Also, it drops the support for ncurses/termcap and uses ansi codes
directly for color support on terminals that support them.

## OpenRC 0.53

The names of cgroups for services started by OpenRC are now prefixed by
"openrc." This is done because some services, like docker, create their
own cgroups.

It is now possible to override the mount options used to mount the
cgroups filesystems.
## OpenRC 0.52

This release drops the "runscript" and "rc" binaries.
These binaries have been deprecated for multiple years, so it should be
fine to remove them at this point.

There was an issue before this release with the default setting for
cgroups being inconsistent. This is fixed.

Start-stop-daemon did not work correctly on Linux 6.6 systems.
This has been fixed in this release as well.

## OpenRC 0.51

The default RC_CGROUP_MODE has been updated to unified.
This benefits users since it will assign each service to its own cgroup,
making resource nanagement better over all.

OUTPUT_LOGGER and ERROR_LOGGER have been implemented for
supervise-daemon. For more information on these settings, please check
the man page.

## OpenRC 0.50

This is a bug fix release which fixes a significant performance issue on
musl libc systems.

## OpenRC 0.49

This release adds support for glibc's builtin 
strlcpy, strlcat etc functions, which will be in posix next.
Also, it fixes completions.


## OpenRC 0.48

This release is a maintenance release; it has no user-facing changes.

## OpenRC 0.47

This release is primarily an internal cleanup release.
The only user-visible difference is that unicode is now on by default.

## OpenRC 0.46

The path for the reference file for the swclock service is now
configurable in conf.d/swclock.

In the past, if supervise_daemon_args was not set *or empty*, it defaulted to
`start_stop_daemon_args`. This was bad because supervise-daemon doesn't
accept the same options as `start-stop-daemon`. So if we set e.g.
`start_stop_daemon_args="--wait 50"`, but not `supervise_daemon_args`,
and the user adds `supervisor=supervise-daemon` to the corresponding
conf.d/<service> file, the service will fail to start due to
unrecognized option "wait".
It would be best to remove this fallback, but that might break some
existing scripts that depend on it. So we are changing it to
use `start_stop_daemon_args` as the default for `supervise_daemon_args`
only if `supervise_daemon_args` is not set at all, but not if it's
empty.

This fallback will be dropped in a future release.


## OpenRC 0.45

The old make-based build system is removed in this release.

The killprocs service now has a KILL_DELAY setting to allow OpenRC based
containers to close all TCP/IP connections before they are shut down.

The --oom-score-adj option has been added to start-stop-daemon and
supervise-daemon. This sets the appropriate setting for the Linux
kernel. for more info, see the man pages.

Support for Linux capabilities has been added to both supervise-daemon
and start-stop-daemon using the --capabilities option.
As a result, the user can specify the inheritable, ambient and bounding set
by defining capabilities in the service script.

noexec has been added to the default mount options for /dev. If you need
to remove this option, add an entry to fstab.

A --secbits option has been added to start-stop-daemon and
supervise-daemon. This sets the security bits option which should be
applied to the daemon.

A no-new-privs option has been added to start-stop-daemon and
supervise-daemon. This sets the NO_NEW_PRIVS flag to apply to the
daemon.

On Linux, the service that seeds the random number generator has been
renamed from urandom to seedrng. This means that when  you upgrade to
this version, if urandom is in your boot runlevel, you must add seedrng.
This can be done by issuing the following commands:

```
# rc-update add seedrng boot
# rc-update del urandom boot
```

## OpenRC 0.44

This version is the first to use a meson-based build system.
The gnu make build system should now be considered deprecated; it will
be removed sometime after 0.44.x.

I have opened a [discussion](https://github.com/OpenRC/openrc/discussions/441)
where you can voice concerns about removing it.

## OpenRC 0.43

This version changes the behavior of the checkpath helper to address
CVE-2018-21269. on Linux systems, We require  non-terminal symbolic links
to be owned by root. Since we can't do this on non-linux systems, we do
not dereference non-terminal symbolic links by default. If you need them
dereferenced, you should add the "-s" switch to the appropriate
checkpath calls.
For more information, see http://github.com/openrc/openrc/issues/201.

The SHLIBDIR variable has been removed from the makefiles to make them
more consistent with most common makefiles. All libraries are now in
LIBDIR, so if you need to put them in /, override the LIBDIR variable
when you run make.

## OpenRC 0.42

openrc-shutdown now has the ability to shut down sysvinit-based systems.

A guide has been added for migrating systems using another init system
to openrc-init.

## OpenRC 0.41.

This version adds the ability to format the output of rc-status when
showing the status of services in a runlevel so that it may be parsed.
Currently, the -f switch only accepts ini as an argument which
causes the output to be in the .ini format.

This version adds an experimental build time switch to allow setting the
default shell to use for service scripts.
By default, this is set to /bin/sh if it is changed, the new shell must
be able to understand posix-compatible syntax.

## OpenRC 0.40

In this version, the keymaps and termencoding services on Linux needed
to be modified so they do not write to the root file system. This was
done so they can run earlier in the boot sequence. AS a result, you will
need to add save-termencoding and save-keymaps to your boot runlevel.
This can be done as follows:

```
# rc-update add save-keymaps boot
# rc-update add save-termencoding boot
```

## OpenRC 0.39

This version removes the support for addons.
The only place I know that this was used was Gentoo Baselayout 1.x, so
it shouldn't affect anyone since baselayout-1 has been dead for a few
years.

Since all supported Linux kernel versions now make efivarfs immutable
and all of the tools that access efivarfs are aware of this, we no
longer mount efivarfs read-only. See the following github issue for more
information:

https://github.com/openrc/openrc/issues/238

This version adds timed shutdown and cancellation of shutdown to
openrc-shutdown. Shutdowns can now be delayed for a certain amount of
time or scheduled for an exact time.

supervise-daemon supports health checks, which are a periodic way to make sure a
service is healthy. For more information on setting this up, please see
supervise-daemon-guide.md.

The --first-time switch has been added to all modprobe commands in the
modules service. This means that, on Linux, you will see failures if a
module was loaded by an initramfs or device manager before this service
runs. These messages are harmless, but to clean them up, you should adjust your
modules autoload configuration.

## OpenRC 0.37

start-stop-daemon now supports logging stdout and stderr of daemons to
processes instead of files. These processes are defined by the
output_logger and error_logger variables in standard service scripts, or
by the  -3/--output-logger or -4/--error-logger switches if you use
start-stop-daemon directly. For more information on this, see the
start-stop-daemon man page.

## OpenRC 0.36

In this release, the modules-load service has been combined into the
modules service since there is no reason I know of to keep them
separate. However, modules also provides modules-load in case you were
using modules-load in  your dependencies.

The consolefont, keymaps, numlock and procfs service scripts no longer
have a dependency on localmount.
If you are a linux user and are still separating / from /usr,
you will need to add the following line to the appropriate conf.d files:

rc_need="localmount"

## OpenRC 0.35

In this version, the cgroups mounting logic has been moved from the
sysfs service to the cgroups service. This was done so cgroups can be
mounted inside an lxc/lxd container without using the other parts of the
sysfs service.

?As a result of this change, if you are upgrading, you need to add
cgroups to your sysinit runlevel by running the following command as
root:

```
# rc-update add cgroups sysinit
```

For more information, see the following issue:

https://github.com/openrc/openrc/issues/187

Consider this your second notification with regard to /etc/mtab being a
file instead of a symbolic link.

In this version, the mtab service will complain loudly if you have
mtab_is_file set to yes and recommend that you change this to no and
restart the mtab service to migrate /etc/mtab to a symbolic link.

If there is a valid technical reason to keep /etc/mtab as a flat file
instead of a symbolic link to /proc/self/mounts, we are interested and
we will keep the support in that case. Please open an issue and let us
know however. Otherwise, consider this your final notice that the mtab
service will be removed in the future.

## OpenRC 0.33

This version removes the "service" binary which was just a copy of
"rc-service" provided for compatibility.

If you still need the "service" binary, as opposed to "rc-service", it is
recommended that you use something like Debian's init-system-helpers.
Otherwise, just use "rc-service" in place of "service".

## OpenRC 0.31

This version adds support for Control Groups version 2, which is
considered stable as of Linux-4.13. Please see /etc/rc.conf for
documentation on how to configure control groups.

## OpenRC-0.28

This version mounts efivars read only due to concerns about changes in
this file system making systems unbootable.  If you need to change something
in this path, you will need to re-mount it read-write, make the change
and re-mount it read-only.

Also, you can override this behavior by adding a line for efivars to
fstab if you want efivars mounted read-write.

For more information on this issue, see the following url:

https://github.com/openrc/openrc/issues/134

## OpenRC-0.25

This version contains an OpenRC-specific implementation of init for
Linux which can be used in place of sysvinit or any other init process.
For information on its usage, see the man pages for openrc-init (8) and
openrc-shutdown (8).

## OpenRC-0.24.1

This version starts cleaning up the dependencies so that rc_parallel
will work correctly.

The first step in this process is to remove the 'before *' from the
depend functions in the clock services. This means some  services not
controlled by OpenRC may now start before instead of after the clock
service. If it is important for these services to start after the clock
service, they need to have 'after clock' added to their depend
functions.

## OpenRC-0.24

Since the deptree2dot tool and the perl requirement are completely
optional, the deptree2dot tool has been moved to the support directory.
As a result, the MKTOOLS=yes/no switch has been removed from the makefiles.

This version adds the agetty service which can be used to spawn
agetty on a specific terminal. This is currently documented in the
agetty-guide.md file at the top level of this distribution.

## OpenRC-0.23

The tmpfiles.d processing code, which was part of previous versions of
OpenRC, has been separated into its own package [1]. If you need to
process systemd style tmpfiles.d files, please install this package.

[1] https://github.com/openrc/opentmpfiles

## OpenRC-0.22

In previous versions of OpenRC, configuration information was processed
so that service-specific configuration stored in /etc/conf.d/* was
overridden by global configuration stored in /etc/rc.conf. This release
reverses that. Global configuration is now overridden by
service-specific configuration.

The swapfiles service, which was basically a copy of the swap service,
has been removed. If you are only using swap partitions, this change
will not affect you. If you are using swap files, please adjust the
dependencies of the swap service as shown in /etc/conf.d/swap.

## OpenRC-0.21

This version adds a daemon supervisor which can start daemons and
restart them if they crash. See supervise-daemon-guide.md in the
distribution for details on its use.

It is now possible to mark certain mount points as critical. If these
mount points are unable to be mounted, localmount or netmount will fail.
This is handled in /etc/conf.d/localmount and /etc/conf.d/netmount. See
these files for the setup.

The deprecation messages in 0.13.x for runscript and rc are now
made visible in preparation for the removal of these binaries in 1.0.

The steps you should take to get rid of these warnings is to run openrc
in initialization steps instead of rc and change the shebang lines in
service scripts to refer to "openrc-run" instead of "runscript".

In 0.21.4, a modules-load service was added. This works like the
equivalent service in systemd. It looks for files named *.conf first in
/usr/lib/modules-load.d, then /run/modules-load.d, then
/etc/modules-load.d. These files contain a list of modules, one per
line, which should be loaded into the kernel. If a file name appears in
/run/modules-load.d, it overrides a file of the same name in
/usr/lib/modules-load.d. A file appearing in /etc/modules-load.d
overrides a file of the same name in both previous directories.

## OpenRC-0.19

This version adds a net-online service. By default, this
service will check all known network interfaces for a configured
interface or a carrier. It will register as started only when all
interfaces are configured and there is at least a carrier on one
interface. The behaviour of this service can be modified in
/etc/conf.d/net-online.

Currently, this only works on Linux, but if anyone wants to port to
*bsd, that would be welcomed.

## OpenRC-0.18.3

Modern Linux systems expect /etc/mtab to be a symbolic link to
/proc/self/mounts. Reasons for this change include support for mount
namespaces, which will not work if /etc/mtab is a file.
By default, the mtab service enforces this on each reboot.

If you find that this breaks your system in some way, please do the
following:

- Set mtab_is_file=yes in /etc/conf.d/mtab.

- Restart mtab. This will recreate the /etc/mtab file.

- Check for an issue on https://github.com/openrc/openrc/issues
  explaining why you need /etc/mtab to be a file. If there isn't one,
  please open one and explain in detail why you need this to be a file.
  If there is one, please add your comments to it. Please give concrete
  examples of why  it is important that /etc/mtab be a file instead of a
  symbolic link. Those comments will be taken into consideration for how
  long to keep supporting mtab as a file or when the support can be
  removed.

## OpenRC-0.18

The behaviour of localmount and netmount in this version is changing. In
the past, these services always started successfully. In this version,
they will be able to fail if file systems they mount fail to mount. If
you have file systems listed in fstab which should not be mounted at
boot time, make sure to add noauto to the mount options. If you have
file systems that you want to attempt to mount at boot time but failure
should be allowed, add nofail to the mount options for these file
systems in fstab.

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
