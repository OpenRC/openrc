if os == 'Linux'
  executable('kill_all',
    ['kill_all.c', rc_exec_c, usage_c, version_h],
    c_args : cc_branding_flags,
    include_directories: [incdir, einfo_incdir, rc_incdir],
    link_with: [libeinfo,librc],
    install: true,
    install_dir: rc_bindir)
endif
