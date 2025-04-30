executable('is_older_than', 'is_older_than.c',
  include_directories: [incdir, einfo_incdir, rc_incdir],
  link_with: [libeinfo, librc],
  dependencies: shared,
  install: true,
  install_dir: rc_bindir)
