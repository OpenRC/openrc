if os == 'Linux'
  executable('openrc-init',
    ['openrc-init.c', rc_exec_c, wtmp_c, version_h],
  c_args : cc_selinux_flags,
    include_directories: [incdir, einfo_incdir, rc_incdir],
    link_with: [libeinfo, librc],
    dependencies: [dl_dep, selinux_dep],
    install: true,
    install_dir: sbindir)
  if get_option('sysvinit')
    install_symlink('init',
      pointing_to: 'openrc-init',
      install_dir: sbindir)
  endif
endif
