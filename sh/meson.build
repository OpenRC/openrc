sh_conf_data = configuration_data()
sh_conf_data.set('BINDIR', bindir)
sh_conf_data.set('LIBEXECDIR', rc_libexecdir)
sh_conf_data.set('LOCAL_PREFIX', local_prefix)
sh_conf_data.set('PKG_PREFIX', pkg_prefix)
sh_conf_data.set('SBINDIR', sbindir)
sh_conf_data.set('SHELL', get_option('shell'))
sh_conf_data.set('SYSCONFDIR', get_option('sysconfdir'))

sh = [
  'rc-functions.sh',
  'rc-mount.sh',
  'runit.sh',
  's6.sh',
  'start-stop-daemon.sh',
  'supervise-daemon.sh',
  ]

scripts_config = [
  'gendepends.sh.in',
  'openrc-run.sh.in',
  'openrc-user.sh.in',
  's6-svscanboot.sh.in'
  ]

if os == 'linux'
  sh += [
    'rc-cgroup.sh',
    ]
  scripts_config += [
    'binfmt.sh.in',
    'cgroup-release-agent.sh.in',
   ]
  scripts_config_os = [
    ['init-early.sh.Linux.in', 'init-early.sh'],
    ['init.sh.Linux.in', 'init.sh'],
    ]
elif os == 'gnu'
  scripts_config_os = [
    ['init.sh.GNU.in', 'init.sh'],
    ]
else
  scripts_config_os = [
    ['init.sh.BSD.in', 'init.sh'],
    ]
endif

install_data(sh, install_dir : rc_shdir)
configure_file(input : 'functions.sh.in', output : '@BASENAME@',
    configuration : sh_conf_data, install_dir : rc_shdir)
install_symlink('functions.sh', pointing_to: rc_shdir / 'functions.sh', install_dir: init_d_dir)

foreach file : scripts_config
  configure_file(input : file,
    output : '@BASENAME@',
    configuration : sh_conf_data,
    install_dir : rc_shdir,
    install_mode : 'rwxr-xr-x')
endforeach

foreach file : scripts_config_os
  configure_file(input : file.get(0),
    output : file.get(1),
    configuration : sh_conf_data,
    install_dir : rc_shdir,
    install_mode : 'rwxr-xr-x')
endforeach
