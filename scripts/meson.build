script_conf_data = configuration_data()
script_conf_data.set('SBINDIR', sbindir)

script_dir = rc_libexecdir / 'bin'

scripts_internal = [
  'on_ac_power',
  ]
  
scripts_Linux = [
  'rc-sstat.in',
  ]

scripts_sysvinit = [
  'halt',
  'poweroff',
  'shutdown',
  'reboot',
  ]

install_data(scripts_internal,
  install_dir : script_dir,
  install_mode: 'rwxr-xr-x')

if os == 'Linux'
  foreach file : scripts_Linux
    configure_file(input : file,
      output : '@BASENAME@',
      configuration : script_conf_data,
      install_dir: script_dir,
      install_mode: 'rwxr-xr-x')
  endforeach
  if get_option('sysvinit')
    foreach file : scripts_sysvinit
      configure_file(input : file,
        output : '@BASENAME@',
        configuration : script_conf_data,
	    install_dir: script_dir,
        install_mode: 'rwxr-xr-x')
    endforeach
  endif
endif

meson.add_install_script('meson_script_links.sh', rc_libexecdir,
	sbindir)
