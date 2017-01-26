# Purpose and description

OpenRC is an init system for Unixoid operating systems. It takes care of 
startup and shutdown of the whole system, including services.

It evolved out of the Gentoo "Baselayout" package which was a custom pure-shell 
startup solution. (This was both hard to maintain and debug, and not very 
performant)

Most of the core parts are written in C99 for performance and flexibility 
reasons, while everything else is posix sh.
The License is 2-clause BSD

Current size is about 10k LoC C, and about 4k LoC shell.

OpenRC is known to work on Linux, many BSDs (FreeBSD, OpenBSD, DragonFlyBSD at 
least) and HURD.

Services are stateful (i.e. `start`; `start` will lead to "it's already started")

# Startup

Usually PID1 (aka. `init`) calls the OpenRC binary (`/sbin/openrc` by default).
(The default setup assumes sysvinit for this)

openrc scans the runlevels (default: `/etc/runlevels`) and builds a dependency
graph, then starts the needed service scripts, either serialized (default) or in 
parallel.

When all the init scripts are started openrc terminates. There is no persistent 
daemon. (Integration with tools like monit, runit or s6 can be done)

# Shutdown

On change to runlevel 0/6 or running `reboot`, `halt` etc., openrc stops all
services that are started and runs the services in the `shutdown` runlevel.

# Modifying Service Scripts

Any service can, at any time, be started/stopped/restarted by executing 
`rc-service someservice start`, `rc-service someservice stop`, etc.
Another, less preferred method, is to run the service script directly,
e.g. `/etc/init.d/service start`, `/etc/init.d/service stop`, etc.

OpenRC will take care of dependencies, e.g starting apache will start network 
first, and stopping network will stop apache first.

There is a special command `zap` that makes OpenRC 'forget' that a service is
started; this is mostly useful to reset a crashed service to stopped state 
without invoking the (possibly broken) stop function of the service script.

Calling `openrc` without any arguments will try to reset all services so
that the current runlevel is satisfied; if you manually started apache it will be 
stopped, and if squid died but is in the current runlevel it'll be restarted.

There is a `service` helper that emulates the syntax seen on e.g. older Redhat
and Ubuntu (`service nginx start` etc.)

# Runlevels

OpenRC has a concept of runlevels, similar to what sysvinit historically 
offered. A runlevel is basically a collection of services that needs to be 
started. Instead of random numbers they are named, and users can create their 
own if needed. This allows, for example, to have a default runlevel with 
"everything" enabled, and a "powersaving" runlevel where some services are 
disabled.

The `rc-status` helper will print all currently active runlevels and the state
of init scripts in them:

```
# rc-status
 * Caching service dependencies ... [ ok ]
Runlevel: default
 modules                     [  started  ]
 lvm                         [  started  ]
```

All runlevels are represented as folders in `/etc/runlevels/` with symlinks to 
the actual init scripts.

Calling openrc with an argument (`openrc default`) will switch to that
runlevel; this will start and stop services as needed.

Managing runlevels is usually done through the `rc-update` helper, but could of 
course be done by hand if desired.
e.g. `rc-update add nginx default` - add nginx to the default runlevel
Note: This will not auto-start nginx! You'd still have to trigger `rc` or run 
the initscript by hand.

FIXME: Document stacked runlevels

The default startup uses the runlevels `boot`, `sysinit` and `default`, in that 
order. Shutdown uses the `shutdown` runlevel.


# Syntax of Service Scripts

Service scripts are shell scripts. OpenRC aims at using only the standardized 
POSIX sh subset for portability reasons. The default interpreter (build-time 
toggle) is `/bin/sh`, so using for example mksh is not a problem.

OpenRC has been tested with busybox sh, ash, dash, bash, mksh, zsh and possibly 
others. Using busybox sh has been difficult as it replaces commands with 
builtins that don't offer the expected features.

The interpreter for initscripts is `#!/sbin/openrc-run`.
Not using this interpreter will break the use of dependencies and is not 
supported. (iow: if you insist on using `#!/bin/sh` you're on your own)

A `depend` function declares the dependencies of this service script.
All scripts must have start/stop/status functions, but defaults are provided.
Extra functions can be added easily:

```
extra_commands="checkconfig"
checkconfig() {
	doSomething
}
```

This exports the checkconfig function so that `/etc/init.d/someservice 
checkconfig` will be available, and it "just" runs this function.

While commands defined in `extra_commands` are always available, commands
defined in `extra_started_commands` will only work when the service is started
and those defined in `extra_stopped_commands` will only work when the service is
stopped. This can be used for implementing graceful reload and similar
behaviour.

Adding a restart function will not work, this is a design decision within 
OpenRC. Since there may be dependencies involved (e.g. network -> apache) a 
restart function is in general not going to work. 
restart is internally mapped to `stop()` + `start()` (plus handling dependencies).
If a service needs to behave differently when it is being restarted vs
started or stopped, it should test the `$RC_CMD` variable, for example:

```
[ "$RC_CMD" = restart ] && do_something
```

# The Depend Function

This function declares the dependencies for a service script. This
determines the order the service scripts start.

```
depend() {
	need net
	use dns logger netmount
	want coolservice
}
```

`need` declares a hard dependency - net always needs to be started before this 
	service does

`use` is a soft dependency - if dns, logger or netmount is in this runlevel 
	start it before, but we don't care if it's not in this runlevel.
	`want` is between need and use - try to start coolservice if it is
	installed on the system, regardless of whether it is in the
	runlevel, but we don't care if it starts.

`before` declares that we need to be started before another service

`after` declares that we need to be started after another service, without 
	creating a dependency (so on calling stop the two are independent)

`provide` allows multiple implementations to provide one service type, e.g.:
	`provide cron` is set in all cron-daemons, so any one of them started 
	satisfies a cron dependency

`keyword` allows platform-specific overrides, e.g. `keyword -lxc` makes this 
	service script a noop in lxc containers. Useful for things like keymaps, 
	module loading etc. that are either platform-specific or not available 
	in containers/virtualization/...

FIXME: Anything missing in this list?

# The Default Functions

All service scripts are assumed to have the following functions:

```
start()
stop()
status()
```

There are default implementations in `lib/rc/sh/openrc-run.sh` - this allows very 
compact service scripts. These functions can be overridden per service script as 
needed.

The default functions assume the following variables to be set in the service 
script:

```
command=
command_args=
pidfile=
```

Thus the 'smallest' service scripts can be half a dozen lines long

# The Magic of `conf.d`

Most service scripts need default values. It would be fragile to
explicitly source some arbitrary files. By convention `openrc-run` will source
the matching file in `/etc/conf.d/` for any script in `/etc/init.d/`

This allows you to set random startup-related things easily. Example:

```
conf.d/foo:
START_OPTS="--extraparameter sausage"

init.d/foo:
start() {
	/usr/sbin/foo-daemon ${STARTOPTS}
}
```

The big advantage of this split is that most of the time editing of the init 
script can be avoided.

# Start-Stop-Daemon

OpenRC has its own modified version of s-s-d, which is historically related and 
mostly syntax-compatible to Debian's s-s-d, but has been rewritten from scratch.

It helps with starting daemons, backgrounding, creating PID files and many 
other convenience functions related to managing daemons.

# `/etc/rc.conf`

This file manages the default configuration for OpenRC, and it has examples of 
per-service-script variables.

Among these are `rc_parallel` (for parallelized startup), `rc_log` (logs all boot 
messages to a file), and a few others.

# ulimit and CGroups

Setting `ulimit` and `nice` values per service can be done through the `rc_ulimit`
variable.

Under Linux, OpenRC can optionally use CGroups for process management.
By default each service script's processes are migrated to their own CGroup.

By changing certain values in the `conf.d` file limits can be enforced per 
service. It is easy to find orphan processes of a service that persist after 
`stop()`, but by default these will NOT be terminated.
To change this add `rc_cgroup_cleanup="yes"` in the `conf.d` files for services 
where you desire this functionality.

# Caching

For performance reasons OpenRC keeps a cache of pre-parsed initscript metadata
(e.g. `depend`). The default location for this is `/${RC_SVCDIR}/cache`.

The cache uses `mtime` to check for file staleness. Should any service script
change it'll re-source the relevant files and update the cache

# Convenience functions

OpenRC has wrappers for many common output tasks in libeinfo.
This allows to print colour-coded status notices and other things.
To make the output consistent the bundled initscripts all use ebegin/eend to 
print nice messages.
