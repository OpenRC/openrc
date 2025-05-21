executable('supervise-daemon', 'supervise-daemon.c',
  dependencies: [rc, einfo, shared, dl_dep, pam_dep, cap_dep, util_dep, selinux_dep],
  include_directories: incdir,
  install: true,
  install_dir: sbindir)

if get_option('pam')
  install_data('supervise-daemon.pam',
    rename : 'supervise-daemon',
    install_dir : pamdir)
endif
