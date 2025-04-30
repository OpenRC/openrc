if os == 'Linux'
  executable('kill_all', 'kill_all.c',
    c_args : cc_branding_flags,
    include_directories: incdir,
    dependencies: [rc, einfo, shared],
    install: true,
    install_dir: rc_bindir)
endif
