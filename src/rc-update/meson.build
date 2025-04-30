executable('rc-update', 'rc-update.c',
  c_args : cc_branding_flags,
  link_with: librc,
  include_directories: [incdir, rc_incdir],
  dependencies: [einfo, shared],
  install: true,
  install_dir: sbindir)
