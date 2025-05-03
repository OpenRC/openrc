if os == 'linux'
  executable('seedrng', 'seedrng.c',
    include_directories: incdir,
    dependencies: [rc, einfo, shared],
    install: true,
    install_dir: rc_sbindir)
endif
