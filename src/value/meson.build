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
    output: exec,
    command: ['ln', '-sf', 'service_get_value', '@OUTPUT@'],
    build_always_stale: true,
    build_by_default: true,
    install: true,
    install_dir: rc_bindir)
endforeach
