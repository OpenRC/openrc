executable('checkpath', 'checkpath.c',
  include_directories: incdir,
  dependencies: [rc, einfo, shared, audit_dep, pam_dep, pam_misc_dep, selinux_dep, crypt_dep],
  install: true,
  install_dir: rc_bindir)
