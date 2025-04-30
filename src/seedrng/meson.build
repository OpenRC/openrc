if os == 'Linux'
  executable('seedrng', 'seedrng.c',
    c_args : cc_branding_flags,
    include_directories: [incdir, einfo_incdir, rc_incdir],
    link_with: [libeinfo, librc],
    dependencies: shared,
    install: true,
    install_dir: rc_sbindir)
endif
