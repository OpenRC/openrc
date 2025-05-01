if os == 'Linux'
  executable('openrc-shutdown',
    ['openrc-shutdown.c', 'broadcast.c', 'sysvinit.c'],
    include_directories: incdir,
    dependencies: [rc, einfo, shared],
    install: true,
    install_dir: sbindir)
endif
