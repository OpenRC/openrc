foreach exec : ['mountinfo', 'do_unmount']
  executable(exec, 'mountinfo.c',
    include_directories: incdir,
    dependencies: [rc, einfo, shared],
    install: true,
    install_dir: rc_bindir)
endforeach
