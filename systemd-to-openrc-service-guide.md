Converting systemd service files to OpenRC service scripts
==========================================================

Most of systemd service configuration options can be
converted to service scripts that OpenRC can understand.

This guide assumes that the user has already read the
`openrc-run` man page and/or have written OpenRC service
scripts before.

Service files installed in /usr/lib/systemd/system
are system services while /usr/lib/systemd/user are
user services and are mapped to /etc/init.d and
/etc/user/init.d respectively.

## Examples

### Example 1
```systemd
Environment=ONE='one' "TWO='two two' too"
ExecStart=/bin/echo ${ONE} ${TWO}
```
Translates to the following OpenRC script:
```sh
#!/sbin/openrc-run

ONE="'one'" TWO="'two two' too"

command=/bin/echo
command_args="${ONE} ${TWO}"
```

### Example 2
```systemd
Environment=ONE='one' "TWO='two two' too" THREE=
ExecStart=/bin/echo ${ONE} ${TWO} ${THREE}
ExecStart=/bin/echo $ONE $TWO $THREE
```
```
#!/sbin/openrc-run

ONE=\'one\' TWO="'two two' too" THREE=

start() {
    /bin/echo ${ONE} ${TWO} ${THREE}
    /bin/echo $ONE $TWO $THREE
}
```

### Example 3
```systemd
Type=oneshot
ExecStart=:echo $USER
ExecStart=-false
ExecStart=+:@true $TEST
```
```sh
#!/sbin/openrc-run

start() {
    echo \$USER
    { false; return 0; } # is this right?
    sudo true ${0} # this is definitely wrong
}
start_post() { mark_service_stopped; }
```

### Example 4
```systemd
ExecStart=echo / >/dev/null & \; \
ls
```
```sh
#!/sbin/openrc-run

start() {
    echo / >/dev/null & \; \
      ls
}
```

### Example 5

Note that this service assumes that `foo-daemon` doesn't
fork/background/daemonizes itself.
```systemd
[Unit]
Description=Foo

[Service]
ExecStart=/usr/sbin/foo-daemon

[Install]
WantedBy=multi-user.target
```
Services that don't daemonize should use the
`supervise-daemon` supervisor.
```sh
#!/sbin/openrc-run

supervisor=supervise-daemon

description="Foo"
command=/usr/sbin/foo-daemon
```

### Example 6
```systemd
[Unit]
Description=Cleanup old Foo data

[Service]
Type=oneshot
ExecStart=/usr/sbin/foo-cleanup

[Install]
WantedBy=multi-user.target
```
```sh
#!/sbin/openrc-run

supervisor=supervise-daemon

description="Cleanup old Foo data"
command=/usr/sbin/foo-cleanup

start_post() {
    mark_service_stopped
}
```

### Example 7
```systemd
[Unit]
Description=Simple firewall

[Service]
Type=oneshot
RemainAfterExit=yes
ExecStart=/usr/local/sbin/simple-firewall-start
ExecStop=/usr/local/sbin/simple-firewall-stop

[Install]
WantedBy=multi-user.target
```
Theres no equivalent to `RemainAfterExit=yes` in OpenRC
```sh
#!/sbin/openrc-run

description="Simple firewall"

start() {
    /usr/local/sbin/simple-firewall-start
}

stop() {
    /usr/local/sbin/simple-firewall-stop
}
```

### Example 8
Traditional daemons that fork themselves (`Type=forking`).
The following example shows a simple daemon that forks
and just starts one process in the background:
```systemd
[Unit]
Description=My Simple Daemon

[Service]
Type=forking
ExecStart=/usr/sbin/my-simple-daemon -d

[Install]
WantedBy=multi-user.target
```
```sh
#!/sbin/openrc-run

description="My simple daemon"
command=/usr/sbin/my-simple-daemon
command_args="-d"
command_background="true"
```

### Example 9

`Type=dbus`
```systemd
[Unit]
Description=Simple DBus Service

[Service]
Type=dbus
BusName=org.example.simple-dbus-service
ExecStart=/usr/sbin/simple-dbus-service

[Install]
WantedBy=multi-user.target
```
Services with `Type=dbus` implicitly have a dependency
on dbus.
```sh
#!/sbin/openrc-run

supervisor=supervise-daemon

description="Simple DBus Service"
command=/usr/sbin/simple-dbus-service

depend() {
    need dbus
}
```

## Real world examples

### /usr/lib/systemd/user/gnome-keyring-daemon.service
```systemd
[Unit]
Description=GNOME Keyring daemon

Requires=gnome-keyring-daemon.socket

[Service]
Type=simple
StandardError=journal
ExecStart=/usr/bin/gnome-keyring-daemon --foreground --components="pkcs11,secrets" --control-directory=%t/keyring
Restart=on-failure

[Install]
Also=gnome-keyring-daemon.socket
WantedBy=default.target
```
```sh
#!/sbin/openrc-run

supervisor=supervise-daemon

description="GNOME Keyring daemon"
command=/usr/bin/gnome-keyring-daemon
command_args="--components=pkcs11,secrets --control-directory=${XDG_RUNTIME_DIR}/keyring"
command_args_foreground="--foreground"
error_logger="logger -t $RC_SVCNAME"
```

### /usr/lib/systemd/user/localsearch-3.service
```systemd
Description=LocalSearch indexer
ConditionUser=!@system
ConditionEnvironment=XDG_SESSION_CLASS=user
After=gnome-session.target

[Service]
Type=notify
BusName=org.freedesktop.LocalSearch3
ExecStart=/usr/libexec/localsearch-3
Restart=on-failure
Slice=background.slice
```
`Busname` means that the service also has `Type=dbus`
```sh
#!/sbin/openrc-run

export DBUS_SESSION_BUS_ADDRESS=unix:path="${XDG_RUNTIME_DIR}"/bus

supervisor=supervise-daemon

description="LocalSearch indexer"
command=/usr/libexec/localsearch-3

depend() {
    need dbus
    after gnome-session
}

start_pre() {
    if [ "${XDG_SESSION_CLASS}" != "user" ]; then
        eerror "XDG_SESSION_CLASS not set to \"user\", exiting"
        return 1
    fi
    # TODO: how to check if the user is a systemd user?
}
```

### /usr/lib/systemd/system/accounts-daemon.service
```systemd
[Unit]
Description=Accounts Service

After=nss-user-lookup.target
Wants=nss-user-lookup.target

[Service]
Type=dbus
BusName=org.freedesktop.Accounts
ExecStart=/usr/libexec/accounts-daemon
Environment=GVFS_DISABLE_FUSE=1
Environment=GIO_USE_VFS=local
Environment=GVFS_REMOTE_VOLUME_MONITOR_IGNORE=1

StateDirectory=AccountsService
StateDirectoryMode=0775

ProtectSystem=strict
PrivateDevices=true
ProtectKernelTunables=true
ProtectKernelModules=true
ProtectControlGroups=true
ProtectHome=false
PrivateTmp=false
PrivateNetwork=true
PrivateUsers=false
RestrictAddressFamilies=AF_UNIX
SystemCallArchitectures=native
SystemCallFilter=~@mount
RestrictNamespaces=true
LockPersonality=true
MemoryDenyWriteExecute=true
RestrictRealtime=true
RemoveIPC=true

ReadWritePaths=\
  -/etc/gdm/custom.conf \
  /etc/ \
  -/var/log/lastlog \
  -/var/log/tallylog \
  -/var/mail/
ReadOnlyPaths=\
  /usr/share/accountsservice/interfaces/ \
  /usr/share/dbus-1/interfaces/ \
  /var/log/wtmp \
  /run/systemd/seats/

[Install]
WantedBy=graphical.target
```
OpenRC doesn't have sandboxing options listed above.
```sh
#!/sbin/openrc-run

GVFS_DISABLE_FUSE=1
GIO_USE_VFS=local
GVFS_REMOTE_VOLUME_MONITOR_IGNORE=1

supervisor=supervise-daemon

description="Accounts Service"
command=/usr/libexec/accounts-daemon

depend() {
    need dbus elogind
}

start_pre() {
    checkpath -d -m 775 /var/lib/AccountsService
}
```

### /usr/lib/systemd/system/thermald.service
```systemd
Description=Thermal Daemon Service
ConditionVirtualization=no

[Service]
Type=dbus
SuccessExitStatus=2
BusName=org.freedesktop.thermald
ExecStart=/usr/bin/thermald --systemd --dbus-enable --adaptive
Restart=on-failure

[Install]
WantedBy=multi-user.target
Alias=dbus-org.freedesktop.thermald.service
```
```sh
#!/sbin/openrc-run

supervisor=supervise-daemon

description="Thermal Daemon Service"
command=/usr/bin/thermald
command_args="--dbus-enable --adaptive"
command_args_foreground="--no-daemon"

depend() {
    need dbus
    keyword -vserver
}
```

### /usr/lib/systemd/system/dnsmasq.service
```systemd
[Unit]
Description=DNS caching server.
Before=nss-lookup.target
Wants=nss-lookup.target
After=network.target
; Use bind-dynamic or uncomment following to listen on non-local IP address
;After=network-online.target

[Service]
ExecStart=/usr/sbin/dnsmasq
Type=forking
PIDFile=/run/dnsmasq.pid

[Install]
WantedBy=multi-user.target
```
```sh
#!/sbin/openrc-run

description="DNS caching server"
command=/usr/sbin/dnsmasq
pidfile=/run/dnsmasq.pid

depend() {
    after net
}
```

### /usr/lib/systemd/system/podman.service
```systemd
[Unit]
Description=Podman API Service
Requires=podman.socket
After=podman.socket
Documentation=man:podman-system-service(1)
StartLimitIntervalSec=0

[Service]
Delegate=true
Type=exec
KillMode=process
Environment=LOGGING="--log-level=info"
ExecStart=/usr/bin/podman $LOGGING system service

[Install]
WantedBy=default.target
```
```sh
#!/sbin/openrc-run

LOGGING="--log-level=info"

supervisor=supervise-daemon

description="Podman API Service"
command=/usr/bin/podman
command_args="$LOGGING system service"
```

## Specifiers

`%a`    `uname -m`

`%A`    `grep ^NAME /etc/os-release`
        `grep "\<VERSION\>" /etc/os-release`
`%C`    `/var/cache` or `$XDG_CACHE_HOME`

`%D`    `/usr/share` or `$XDG_DATA_HOME`

`%E`    `/etc` or `$XDG_CONFIG_HOME`

`%h`    `$HOME`

`%H`    hostname

`%L`    `/var/log`

`%n`    `$RC_SVCNAME`

`%N`	`$RC_SVCNAME`

`%s`    `$SHELL`

`%S` or `StateDirectory`    `/var/lib` or `$XDG_STATE_HOME`

`%t`    `/run` or `$XDG_RUNTIME_DIR`

`%T`    `/tmp`, `$TMPDIR`, `$TEMP` or `$TMP`

`%u`    $USER

`%U`    `$UID` or $(id -u)
