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
  executable(exec,
    ['einfo.c', version_h],
    include_directories: [incdir, einfo_incdir, rc_incdir],
    link_with: [libeinfo, librc],
    install: true,
    install_dir: rc_bindir)
endforeach
