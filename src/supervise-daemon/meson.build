executable('supervise-daemon',
  ['supervise-daemon.c', rc_exec_c, misc_c, plugin_c, schedules_c, timeutils_c, usage_c, version_h],
  c_args : [cc_branding_flags, cc_pam_flags, cc_selinux_flags],
  link_with: [libeinfo, librc],
  dependencies: [dl_dep, pam_dep, cap_dep, util_dep, selinux_dep],
  include_directories: [incdir, einfo_incdir, rc_incdir],
  install: true,
  install_dir: sbindir)

if get_option('pam')
  install_data('supervise-daemon.pam',
    rename : 'supervise-daemon',
    install_dir : pamdir)
endif
