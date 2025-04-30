executable('openrc-user', 'openrc-user.c',
  c_args: [cc_pam_flags],
  dependencies: [rc, einfo, shared, pam_dep],
  include_directories: incdir,
  install: true,
  install_dir: rc_bindir)
