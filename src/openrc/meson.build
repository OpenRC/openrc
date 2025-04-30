executable('openrc', ['rc.c', 'rc-logger.c'],
  c_args : cc_branding_flags,
  link_with: [libeinfo, librc],
  dependencies: [shared, dl_dep, util_dep],
  include_directories: [incdir, einfo_incdir, rc_incdir],
  install: true,
  install_dir: sbindir)
