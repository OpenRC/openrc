service_execs = [
  'service_starting',
  'service_started',
  'service_stopping',
  'service_stopped',
  'service_inactive',
  'service_wasinactive',
  'service_hotplugged',
  'service_started_daemon',
  'service_crashed',
  ]

foreach exec : service_execs
  executable(exec,
    ['service.c', misc_c, version_h],
    include_directories: [incdir, einfo_incdir, rc_incdir],
    link_with: [libeinfo, librc],
    install: true,
    install_dir: rc_bindir)
endforeach
