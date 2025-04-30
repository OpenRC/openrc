executable('openrc-run', 'openrc-run.c',
  c_args : [cc_audit_flags, cc_branding_flags, cc_pam_flags, cc_selinux_flags],
  dependencies: [einfo, shared, audit_dep, dl_dep, pam_dep, pam_misc_dep, selinux_dep, util_dep, crypt_dep],
  include_directories: [incdir, rc_incdir],
  install: true,
  install_dir: sbindir)
