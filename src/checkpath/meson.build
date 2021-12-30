executable('checkpath',
  ['checkpath.c', misc_c, usage_c, selinux_c,
    version_h],
  c_args : [cc_audit_flags, cc_branding_flags, cc_pam_flags, cc_selinux_flags],
  include_directories: [incdir, einfo_incdir, rc_incdir],
  link_with: [libeinfo, librc],
  dependencies: [audit_dep, pam_dep, pam_misc_dep, selinux_dep, crypt_dep],
  install: true,
  install_dir: rc_bindir)
