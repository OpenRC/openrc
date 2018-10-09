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

# Health Checks

Health checks are a way to make sure a service monitored by
supervise-daemon stays healthy. To configure a health check for a
service, you need to write a healthcheck() function, and optionally an
unhealthy() function in the service script. Also, you will need to set
the healthcheck_timer and optionally healthcheck_delay variables.

## healthcheck() function

The healthcheck() function is run repeatedly based on the settings of
the healthcheck_* variables. This function should return zero if the
service is currently healthy or non-zero otherwise.

## unhealthy() function

If the healthcheck() function returns non-zero, the unhealthy() function
is run, then the service is restarted. Since the service will be
restarted by the supervisor, the unhealthy function should not try to
restart it; the purpose of the function is to allow any cleanup tasks
other than restarting the service to be run.

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
healthcheck_delay=seconds
```

This is the delay, in seconds, before the first health check is run.
If it is not set, we use the value of healthcheck_timer.

``` sh
healthcheck_timer=seconds
```

This is the  number of seconds between health checks. If it is not set,
no health checks will be run.

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
