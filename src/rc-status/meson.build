executable('rc-status', 'rc-status.c',
  c_args : cc_branding_flags,
  link_with: librc,
  dependencies: [einfo, shared, util_dep],
  include_directories: [incdir, rc_incdir],
  install: true,
  install_dir: bindir)
