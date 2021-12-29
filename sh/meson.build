sh_conf_data = configuration_data()
if rootprefix == '/'
  sh_conf_data.set('PREFIX', '')
  else
  sh_conf_data.set('PREFIX', rootprefix)
endif
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

sh_config = [
  'functions.sh.in',
  ]

scripts_config = [
  'gendepends.sh.in',
  'openrc-run.sh.in',
  ]

if os == 'Linux'
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
elif os == 'GNU'
  scripts_config_os = [
    ['init.sh.GNU.in', 'init.sh'],
    ]
elif os == 'Gnu-KFreeBSD'
  scripts_config_os = [
    ['init.sh.GNU-kFreeBSD.in', 'init.sh'],
    ]
else
  scripts_config_os = [
    ['init.sh.BSD.in', 'init.sh'],
    ]
endif

install_data(sh,
  install_dir : rc_shdir)
foreach file : sh_config
  configure_file(input : file,
    output : '@BASENAME@',
    configuration : sh_conf_data,
    install_dir : rc_shdir)
endforeach

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
