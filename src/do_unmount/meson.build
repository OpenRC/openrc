executable('do_unmount',
  ['do_unmount.c'],
  include_directories: [incdir, einfo_incdir, rc_incdir],
  link_with: [libeinfo, librc],
  dependencies: thread_dep,
  install: true,
  install_dir: rc_bindir)
