libeinfo_version = '1'

libeinfo = library('einfo', ['libeinfo_new.c'],
  # TODO: remove -Werror
  c_args : [curses_flags, '-Werror'],
  include_directories : incdir,
  dependencies : curses_dep,
  link_depends : 'einfo.map',
  version : libeinfo_version,
  install : true,
  install_dir : libdir)

install_headers('einfo.h')
