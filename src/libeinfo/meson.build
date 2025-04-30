einfo = declare_dependency(
  include_directories: '.',
  link_with: library('einfo', 'libeinfo.c',
    include_directories: incdir,
    link_depends: 'einfo.map',
    install_dir: libdir,
    install: true,
    version: '1'))

install_headers('einfo.h')
