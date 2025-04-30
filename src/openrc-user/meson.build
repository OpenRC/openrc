executable('openrc-user', 'openrc-user.c',
  c_args: [cc_pam_flags],
  dependencies: [shared, pam_dep],
  include_directories: [incdir, einfo_incdir, rc_incdir],
  link_with: [libeinfo, librc],
  install: true,
  install_dir: rc_bindir)
