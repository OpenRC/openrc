if os == 'Linux'
  executable('kill_all', 'kill_all.c',
    c_args : cc_branding_flags,
    include_directories: [incdir, einfo_incdir, rc_incdir],
    link_with: [libeinfo,librc],
    dependencies: shared,
    install: true,
    install_dir: rc_bindir)
endif
