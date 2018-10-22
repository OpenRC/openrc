Using supervise-daemon
======================

Beginning with OpenRC-0.21 we have our own daemon supervisor,
supervise-daemon., which can start a daemon and restart it if it
terminates unexpectedly.

The following is a brief guide on using this capability.

* Use Default start, stop and status functions
  If you write your own start, stop and status functions in your service
  script, none of this will work. You must allow OpenRC to use the default
  functions.

* Daemons must not fork
  Any deamon that you would like to have monitored by supervise-daemon
  must not fork. Instead, it must stay in the foreground. If the daemon
  forks, the supervisor will be unable to monitor it.

  If the daemon can be configured to not fork, this should be done in the
  daemon's configuration file, or by adding a command line option that
  instructs it not to fork to the command_args_foreground variable shown
  below.

# Variable Settings

The most important setting is the supervisor variable. At the top of
your service script, you should set this variable as follows:

``` sh
supervisor=supervise-daemon
```

Several other variables affect the way services behave under
supervise-daemon. They are documented on the  openrc-run man page, but I
will list them here for convenience:

``` sh
pidfile=/pid/of/supervisor.pid
```

If you are using start-stop-daemon to monitor your scripts, the pidfile
is the path to the pidfile the daemon creates. If, on the other hand,
you are using supervise-daemon, this is the path to the pidfile the
supervisor creates.

``` sh
command_args_foreground="arguments"
```

This 	should be used if the daemon you want to monitor
forks and goes to the background by default. This should be set to the
command line option that instructs the daemon to stay in the foreground.

``` sh
respawn_delay
```

This is the number of seconds to delay before attempting to respawn a
supervised process after it dies unexpectedly.
The default is to respawn immediately.

``` sh
respawn_max=x
```

This is the maximum number of times to respawn a supervised process
during the given respawn period. The default is unlimited.

``` sh
respawn_period=seconds
```

This works in conjunction with respawn_max and respawn_delay above to
decide if a process should not be respawned for some reason.

For example, if respawn_period is 60, respawn_max is 2 and respawn_delay
is 3 and a process dies more than 4 times, the process will not be
respawned and the supervisor will terminate.
