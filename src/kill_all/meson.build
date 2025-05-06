if os == 'linux'
  executable('kill_all', 'kill_all.c',
    include_directories: incdir,
    dependencies: [rc, einfo, shared],
    install: true,
    install_dir: rc_bindir)
endif
