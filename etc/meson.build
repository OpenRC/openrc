etc_conf_data = configuration_data()
if os == 'freebsd'
  etc_conf_data.set('TERM', 'cons25')
elif os == 'linux'
  etc_conf_data.set('TERM', 'wsvt25')
endif

etc_conf_common = [
  'rc.conf',
  ]

etc_bin_FreeBSD = [
  'rc.devd',
  ]

etc_conf_FreeBSD = [
  'devd.conf',
  ]

etc_bin_NetBSD =  [
  'rc.in',
  'rc.shutdown.in',
  ]

install_data(etc_conf_common,
  install_dir : get_option('sysconfdir'))

  if os == 'freebsd'
  install_data(etc_bin_FreeBSD,
    install_dir : get_option('sysconfdir'),
    install_mode: 'rwxr-xr-x')
  install_data(etc_conf_FreeBSD,
    install_dir : get_option('sysconfdir'))
endif

if os == 'freebsd' or os == 'netbsd'
  foreach file : etc_bin_NetBSD
    configure_file(input : file,
      output : '@BASENAME@',
      configuration : etc_conf_data,
      install_dir: get_option('sysconfdir'),
      install_mode: 'rwxr-xr-x')
  endforeach
endif
