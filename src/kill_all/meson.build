if os == 'Linux'
  executable('kill_all', 'kill_all.c',
    c_args : cc_branding_flags,
    include_directories: [incdir, rc_incdir],
    link_with: librc,
    dependencies: [einfo, shared],
    install: true,
    install_dir: rc_bindir)
endif
