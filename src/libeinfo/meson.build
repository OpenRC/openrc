libeinfo_version = '1'

libeinfo = library('einfo', ['libeinfo.c'],
  include_directories : incdir,
  link_depends : 'einfo.map',
  version : libeinfo_version,
  install : true,
  install_dir : libdir)

install_headers('einfo.h')
