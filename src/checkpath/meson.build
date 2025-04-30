executable('checkpath', 'checkpath.c',
  c_args : [cc_audit_flags, cc_branding_flags, cc_pam_flags, cc_selinux_flags],
  include_directories: [incdir, einfo_incdir, rc_incdir],
  link_with: [libeinfo, librc],
  dependencies: [shared, audit_dep, pam_dep, pam_misc_dep, selinux_dep, crypt_dep],
  install: true,
  install_dir: rc_bindir)
