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
    output: exec,
    command: ['ln', '-sf', 'service_starting', '@OUTPUT@'],
    build_always_stale: true,
    build_by_default: true,
    install: true,
    install_dir: rc_bindir)
endforeach
