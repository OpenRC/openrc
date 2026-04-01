if os == 'linux'
  executable('sysv-initctl', 'initctl.c',
    include_directories: incdir,
    dependencies: rc,
    install: true,
    install_dir: rc_sbindir)
endif
