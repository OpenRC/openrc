value_execs = [
  'service_get_value',
  'service_set_value',
  'get_options',
  'save_options',
  ]

foreach exec : value_execs
  executable(exec, 'value.c',
    include_directories: incdir,
    dependencies: [rc, einfo, shared],
    install: true,
    install_dir: rc_bindir)
endforeach
