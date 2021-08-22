conf_d_dir = get_option('sysconfdir') / 'conf.d'

conf_common = [
  'bootmisc',
  'fsck',
  'hostname',
  'localmount',
  'netmount',
  'swap',
  'urandom',
  ]

conf_net = [
  'network',
  'staticroute',
  ]

conf_FreeBSD = [
  'ipfw',
  'modules',
  'moused',
  'powerd',
  'rarpd',
  'savecore',
  'syscons',
  ]

conf_Linux = [
  'agetty',
  'consolefont',
  'devfs',
  'dmesg',
  'hwclock',
  'keymaps',
  'killprocs',
  'modules',
  'mtab',
  'net-online',
  ]

conf_NetBSD =  [
  'moused',
  'rarpd',
  'savecore',
  ]

conf_data = conf_common
if get_option('newnet')
  conf_data = conf_data + conf_net
endif
if os == 'FreeBSD'
  conf_data = conf_data + conf_FreeBSD
elif os == 'Linux'
  conf_data = conf_data + conf_Linux
elif os == 'NetBSD'
  conf_data = conf_data + conf_NetBSD
endif

install_data(conf_data, install_dir : conf_d_dir)
