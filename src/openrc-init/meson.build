if os == 'Linux'
  executable('openrc-init', 'openrc-init.c',
    include_directories: incdir,
    dependencies: [rc, einfo, shared, dl_dep, selinux_dep],
    install: true,
    install_dir: sbindir)
  if get_option('sysvinit')
    install_symlink('init',
      pointing_to: 'openrc-init',
      install_dir: sbindir)
  endif
endif
