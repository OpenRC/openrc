service_execs = [
  'service_started',
  'service_stopping',
  'service_stopped',
  'service_inactive',
  'service_wasinactive',
  'service_hotplugged',
  'service_started_daemon',
  'service_crashed',
  ]

executable('service_starting',
  ['service.c', misc_c, version_h],
  include_directories: [incdir, einfo_incdir, rc_incdir],
  link_with: [libeinfo, librc],
  install: true,
  install_dir: rc_bindir)

foreach exec : service_execs
  custom_target(exec,
    build_by_default: true,
    command: ['ln', '-fns', 'service_starting', '@OUTPUT@'],
    output: exec)
  install_symlink(exec,
    install_dir: rc_bindir,
    pointing_to: 'service_starting')
endforeach
