if os == 'Linux'
  executable('openrc-shutdown',
    ['openrc-shutdown.c', 'broadcast.c', 'sysvinit.c'],
    c_args : cc_branding_flags,
    include_directories: [incdir, einfo_incdir, rc_incdir],
    link_with: [libeinfo, librc],
    dependencies: shared,
    install: true,
    install_dir: sbindir)
endif
