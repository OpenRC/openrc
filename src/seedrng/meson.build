if os == 'Linux'
  executable('seedrng', 'seedrng.c',
    c_args : cc_branding_flags,
    include_directories: incdir,
    dependencies: [rc, einfo, shared],
    install: true,
    install_dir: rc_sbindir)
endif
