executable('openrc-user', 'openrc-user.c',
  c_args: [cc_pam_flags],
  dependencies: [einfo, shared, pam_dep],
  include_directories: [incdir, rc_incdir],
  link_with: librc,
  install: true,
  install_dir: rc_bindir)
