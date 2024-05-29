mark_service_execs =	[
  'mark_service_started',
  'mark_service_stopping',
  'mark_service_stopped',
  'mark_service_inactive',
  'mark_service_wasinactive',
  'mark_service_hotplugged',
  'mark_service_failed',
  'mark_service_crashed',
  ]

mark_service = executable('mark_service_starting',
  ['mark_service.c', misc_c, version_h],
  include_directories: [incdir, einfo_incdir, rc_incdir],
  link_with: [libeinfo,librc],
  install: true,
  install_dir: rc_sbindir)

foreach exec : mark_service_execs
  custom_target(exec,
    build_by_default: true,
    command: ['ln', '-fn', mark_service, '@OUTPUT@'],
    output: exec)
endforeach

meson.add_install_script(install_hardlinks,
  rc_sbindir,
  'mark_service_starting',
  mark_service_execs)
