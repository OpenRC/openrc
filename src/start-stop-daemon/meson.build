executable('start-stop-daemon',
  ['start-stop-daemon.c', rc_exec_c, misc_c, schedules_c,
	selinux_c, usage_c, version_h],
  c_args : [cc_audit_flags, cc_branding_flags, cc_pam_flags, cc_selinux_flags],
  link_with: [libeinfo, librc],
  dependencies: [audit_dep, dl_dep, pam_dep, cap_dep, pam_misc_dep, util_dep, selinux_dep, crypt_dep, dependency('threads')],
  include_directories: [incdir, einfo_incdir, rc_incdir],
  install: true,
  install_dir: sbindir)

if get_option('pam')
  install_data('start-stop-daemon.pam',
    rename : 'start-stop-daemon',
    install_dir : pamdir)
endif
