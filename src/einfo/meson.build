einfo_execs = [
  'einfon',
  'ewarnn',
  'ewarn',
  'eerrorn',
  'eerror',
  'ebegin',
  'eend',
  'ewend',
  'eindent',
  'eoutdent',
  'esyslog',
  'eval_ecolors',
  'ewaitfile',
  'veinfo',
  'vewarn',
  'vebegin',
  'veend',
  'vewend',
  'veindent',
  'veoutdent',
  ]

executable('einfo',
  ['einfo.c', version_h],
  include_directories: [incdir, einfo_incdir, rc_incdir],
  link_with: [libeinfo, librc],
  install: true,
  install_dir: rc_bindir)

foreach exec: einfo_execs
  custom_target(exec,
    build_by_default: true,
    command: ['ln', '-fns', 'einfo', '@OUTPUT@'],
    output: exec)
  install_symlink(exec,
    install_dir: rc_bindir,
    pointing_to: 'einfo')
endforeach
