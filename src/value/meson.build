value_execs = [
  'service_set_value',
  'get_options',
  'save_options',
  ]

executable('service_get_value',
  ['value.c', misc_c, version_h],
  include_directories: [incdir, einfo_incdir, rc_incdir],
  link_with: [libeinfo, librc],
  install: true,
  install_dir: rc_bindir)

foreach exec : value_execs
  custom_target(exec,
    build_by_default: true,
    command: ['ln', '-fns', 'service_get_value', '@OUTPUT@'],
    output: exec)
  install_symlink(exec,
    install_dir: rc_bindir,
    pointing_to: 'service_get_value')
endforeach
