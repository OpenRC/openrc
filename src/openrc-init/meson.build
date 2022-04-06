if os == 'Linux'
  executable('openrc-init',
    ['openrc-init.c', plugin_c, wtmp_c, version_h],
  c_args : cc_selinux_flags,
    include_directories: [incdir, einfo_incdir, rc_incdir],
    link_with: [libeinfo, librc],
    dependencies: [dl_dep, selinux_dep],
    install: true,
    install_dir: sbindir)
endif
