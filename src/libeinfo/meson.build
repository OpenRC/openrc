libeinfo_version = '1'

libeinfo = library('einfo', ['libeinfo.c'],
  c_args : termcap_flags,
  include_directories : incdir,
  dependencies : termcap_dep,
  link_depends : 'einfo.map',
  version : libeinfo_version,
  install : true,
  install_dir : libdir)

install_headers('einfo.h')
