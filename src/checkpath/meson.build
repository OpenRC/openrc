executable('checkpath', 'checkpath.c',
  c_args : [cc_audit_flags, cc_branding_flags, cc_pam_flags, cc_selinux_flags],
  include_directories: incdir,
  dependencies: [rc, einfo, shared, audit_dep, pam_dep, pam_misc_dep, selinux_dep, crypt_dep],
  install: true,
  install_dir: rc_bindir)
