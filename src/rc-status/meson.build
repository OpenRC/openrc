executable('rc-status', 'rc-status.c',
  c_args : cc_branding_flags,
  link_with: [libeinfo, librc],
  dependencies: [shared, util_dep],
  include_directories: [incdir, einfo_incdir, rc_incdir],
  install: true,
  install_dir: bindir)
