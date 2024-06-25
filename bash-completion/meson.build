if get_option('bash-completions')
bash_completions = [
  'openrc',
  'rc-service',
  'rc-status',
  'rc-update',
  ]

install_data(bash_completions,
  install_dir : get_option('datadir') / 'bash-completion/completions')
  install_data(
    'openrc-service-script',
  install_dir : get_option('sysconfdir') / 'bash_completion.d')
endif
