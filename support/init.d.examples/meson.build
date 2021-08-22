init_d_examples_dir = support_dir / 'init.d.examples'

init_d_examples =  [
  'avahi-dnsconfd.in',
  'avahid.in',
  'dhcpcd.in',
  'dbus.in',
  'hald.in',
  'named.in',
  'ntpd.in',
  'openvpn.in',
  'polkitd.in',
  'sshd.in',
  'wpa_supplicant.in',
  ]

init_d_examples_data = [
  'README.md',
  ]

foreach init_d_example: init_d_examples
  configure_file(input : init_d_example,
    output : '@BASENAME@',
    configuration : init_d_conf_data,
    install_dir : init_d_examples_dir,
    install_mode : 'rwxr-xr-x')
endforeach

install_data(init_d_examples_data,
  install_dir : init_d_examples_dir)
