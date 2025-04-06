executable('openrc-run',
  ['openrc-run.c', misc_c, timeutils_c, plugin_c, rc_exec_c, selinux_c, usage_c, version_h],
  c_args : [cc_audit_flags, cc_branding_flags, cc_pam_flags, cc_selinux_flags],
  link_with: [libeinfo, librc],
  dependencies: [audit_dep, dl_dep, pam_dep, pam_misc_dep, selinux_dep, util_dep, crypt_dep],
  include_directories: [incdir, einfo_incdir, rc_incdir],
  install: true,
  install_dir: sbindir)
