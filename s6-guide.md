Using s6 with OpenRC
====================

Beginning with OpenRC-0.16, we support using the s6 supervision suite
from skarnet.org in place of start-stop-daemon for monitoring daemons [1].


## Setup

Documenting s6 in detail is beyond the scope of this guide. It will
document how to set up OpenRC services to communicate with s6.

### Use Default start, stop and status functions

If you write your own start, stop and status functions in your service
script, none of this will work. You must allow OpenRC to use the default
functions.

### Dependencies

All OpenRC service scripts that want their daemons monitored by s6
may have the following line added to their dependencies to make sure
the s6 scan directory is being monitored.

need s6-svscan

Starting with 0.63, it is not necessary to manually add this dependency.
OpenRC should deduce it automatically if your service file includes the
supervisor=s6 line. (See right below.)

### Variable Settings

The most important setting is the supervisor variable. At the top of
your service script, you should set this variable as follows:

supervisor=s6

Several other variables affect s6 services. Some of them are generic,
whether you're using start-stop-daemon, supervise-daemon or s6:

command, command_args, command_args_foreground: these variables  will be
run as a shell command line. Do not start with "exec", the s6 backend
will add it automatically.

Some variables are used with supervisor=s6 the same way as with
supervisor=supervise-daemon:

output_logger, error_logger (mutually exclusive, you can only have
one logger; error_logger will log _both_ stdout and stderr, unless
you also define an output_log which will log your stdout separately
to a file.)

input_file, output_log, error_log (a corresponding _logger program
overrides a _log file)

directory, chroot, umask, command_user (command_user must be a username
appearing in your user database, not a numerical uid)

notify (only the fd:X method is supported: when ready, your daemon must
write a (possibly empty) line to file descriptor X)

stopsig (accepts signal names and numbers)

Some variables are specific to the supervisor=s6 backend:

error_logger=auto, s6_log_arguments: if you set your error_logger
(or your output_logger, but error_logger is recommended) to "auto",
then a logger service is automatically built using the s6-log
program, logging to the /var/log/$RC_SVCNAME directory with TAI64N
timestamps, automatic rotations every 1 MiB of data, and a maximum
of 10 archived files. You can fine-tune the settings given to s6-log
via the s6_log_arguments variable (but not the logging directory;
for more control you can still set error_logger manually, even to
an s6-log command line!)

timeout_ready=N: wait for up to N milliseconds for the service to
become up, and fail otherwise. If notify=fd:X has been set, it waits
for the service to be _ready_ instead, which is what you really want.
If N=0, OpenRC will wait indefinitely until the service is up/ready.
If this variable is not set, it will not wait at all and report
success as soon as the command to bring the service up has
successfully been sent.

timeout_down=N: wait for up to N milliseconds when stopping the service,
until s6 reports it as down. If it fails, the service might still be
successfully brought down. If N=0, OpenRC will wait indefinitely for
the service to die (this works well in conjunction with timeout_kill,
see below); if the variable is unset, it will not wait at all.

timeout_kill=N: when openrc tries to stop the service, s6 sends a SIGTERM
(or the value of stopsig). If that signal has not managed to bring the
service down after N milliseconds, a SIGKILL will be sent. This is
useful when you want to make sure your service is down; use in
conjunction with timeout_down. N=0 makes no sense here. Not setting
timeout_kill means no SIGKILL will be sent.


## How it works internally, starting with 0.63

If the `command` variable is empty *and* there is a user-provided
service directory in `/var/svc.d` with the same name as the service
being called, then everything works as it did previously: the
`/var/svc.d/foo` service directory is linked into the scan directory,
and that's it: you are in full manual control of your service directory.
You can still use the `timeout_ready` and `timeout_down` variables to
tune OpenRC's behaviour, but the other variables have no impact.

The rest of this section assumes that the `command` variable is not
empty. In that case, you don't need to provide a service directory in
the s6 format: OpenRC will craft one for you.

The first time start() is called, OpenRC uses all the variables in the
service file to build a service directory: a run script, possibly a
notification-fd file, etc. This service directory is then linked into
the scan directory and s6-svscan is told to register it and spawn a
s6-supervise process on it.

This means that all the information needed for your service should be
given, declaratively, in your service file (and your configuration file
if you have one). In true OpenRC fashion, the service file is the One
True Source of information for running your service.

The run script for the s6 service directory is built with in the
execline language, because execline makes script generation easier
than sh. However, the daemon execution itself is still done via
  `sh -c "$command $command_args $command_args_foreground"`
for compatibility with other backends. In other words: you can forget
that execline is even there, all the user-facing parts use sh as their
interpreter and it's all you need to worry about.

When the service is stopped, the service directory is unlinked from the
scan directory, but the service directory itself remains. If the service
is started again, the same service directory is linked again: it does
not need to be rebuilt, unless the service file (typically in /etc/init.d)
or the configuration file (typically in /etc/conf.d) have changed since,
in which case it is created anew.

On shutdown, the existing service directories (minus their runtime data)
are stored into the OpenRC cache, and restored at the next boot.

### Logging

If you don't set a logger at all, the stdout and stderr of your service
will fall through to the catch-all logger of the s6-svscan service, which
logs all s6 services that don't have a dedicated logger. These logs are
accessible in /run/openrc/s6-logs (or whatever $RC_SVCDIR/s6-logs is on
your machine).


## Future direction

The s6 backend in OpenRC aims to be as close as possible to the
supervise-daemon backend, and most services should be able to use one
or the other interchangeably. More service file variables may be supported
in the future as supervise-daemon evolves; they will be documented in this
file. There is a also possibility that the preferred interface for defining
a service command line changes in a future version of OpenRC; in that case,
the change will apply to both supervise-daemon and s6 equally.


[1] https://skarnet.org/software/s6/
