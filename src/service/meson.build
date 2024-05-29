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

service = executable('service_starting',
  ['service.c', misc_c, version_h],
  include_directories: [incdir, einfo_incdir, rc_incdir],
  link_with: [libeinfo, librc],
  install: true,
  install_dir: rc_bindir)

foreach exec : service_execs
  custom_target(exec,
    build_by_default: true,
    command: ['ln', '-fn', service, '@OUTPUT@'],
    output: exec)
endforeach

meson.add_install_script(install_hardlinks,
  rc_bindir,
  'service_starting',
  service_execs)
