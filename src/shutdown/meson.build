if os == 'Linux' and get_option('sysvinit')
  configure_file(input : 'shutdown.in',
    output : '@BASENAME@',
    configuration : script_conf_data,
    install_dir: sbindir,
    install_mode: 'rwxr-xr-x')
endif
