executable('rc-update', 'rc-update.c',
  c_args : cc_branding_flags,
  link_with: [libeinfo, librc],
  include_directories: [incdir, einfo_incdir, rc_incdir],
  dependencies: [shared],
  install: true,
  install_dir: sbindir)
