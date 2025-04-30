executable('openrc', ['rc.c', 'rc-logger.c'],
  c_args : cc_branding_flags,
  link_with: librc,
  dependencies: [einfo, shared, dl_dep, util_dep],
  include_directories: [incdir, rc_incdir],
  install: true,
  install_dir: sbindir)
