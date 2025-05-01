executable('start-stop-daemon', 'start-stop-daemon.c',
  dependencies: [rc, einfo, shared, audit_dep, dl_dep, pam_dep, cap_dep, pam_misc_dep, util_dep, selinux_dep, crypt_dep, dependency('threads')],
  include_directories: incdir,
  install: true,
  install_dir: sbindir)

if get_option('pam')
  install_data('start-stop-daemon.pam',
    rename : 'start-stop-daemon',
    install_dir : pamdir)
endif
