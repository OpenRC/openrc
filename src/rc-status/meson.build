executable('rc-status',
  ['rc-status.c', misc_c, usage_c, version_h],
  c_args : cc_branding_flags,
  link_with: [libeinfo, librc],
  dependencies: [util_dep],
  include_directories: [incdir, einfo_incdir, rc_incdir],
  install: true,
  install_dir: bindir)
