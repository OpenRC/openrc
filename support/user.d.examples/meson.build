user_d_examples_dir = support_dir / 'user.d.examples'

user_d_examples =  [
    'dbus.in',
    'dunst.in',
    'pipewire.in',
    'pipewire-pulse.in',
    'ssh-agent.in',
    'wireplumber.in',
]

user_d_examples_data = [
  'README.md',
]

foreach user_d_example: user_d_examples
  configure_file(input : user_d_example,
    output : '@BASENAME@',
    configuration : init_d_conf_data,
    install_dir : user_d_examples_dir,
    install_mode : 'rwxr-xr-x')
endforeach

install_data(user_d_examples_data,
  install_dir : user_d_examples_dir)
