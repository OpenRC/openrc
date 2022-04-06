value_execs = [
  'service_get_value',
  'service_set_value',
  'get_options',
  'save_options',
  ]

foreach exec : value_execs
  executable(exec,
    ['value.c', misc_c, version_h],
    include_directories: [incdir, einfo_incdir, rc_incdir],
    link_with: [libeinfo, librc],
    install: true,
    install_dir: rc_bindir)
endforeach
