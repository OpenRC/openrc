einfo_execs = [
  'einfon',
  'einfo',
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

foreach exec: einfo_execs
  executable(exec, 'einfo.c',
    include_directories: [incdir, rc_incdir],
    link_with: librc,
    dependencies: einfo,
    install: true,
    install_dir: rc_bindir)
endforeach
