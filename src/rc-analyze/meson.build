libm = cc.find_library('m', required: true)

executable('rc-analyze', 'rc-analyze.c',
  dependencies: [rc, einfo, shared, libm],
  include_directories: incdir,
  install: true,
  install_dir: bindir)
