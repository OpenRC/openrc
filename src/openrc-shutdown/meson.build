if os == 'Linux'
  executable('openrc-shutdown',
    ['openrc-shutdown.c', 'broadcast.c', 'rc-sysvinit.c', misc_c,
		usage_c, wtmp_c, version_h],
    c_args : cc_branding_flags,
    include_directories: [incdir, einfo_incdir, rc_incdir],
    link_with: [libeinfo, librc],
    install: true,
    install_dir: sbindir)
endif
