executable('openrc', ['rc.c', 'rc-logger.c'],
  c_args : cc_branding_flags,
  dependencies: [rc, einfo, shared, dl_dep, util_dep],
  include_directories: incdir,
  install: true,
  install_dir: sbindir)
