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

einfo = executable('einfo',
  ['einfo.c', version_h],
  include_directories: [incdir, einfo_incdir, rc_incdir],
  link_with: [libeinfo, librc],
  install: true,
  install_dir: rc_bindir)

foreach exec: einfo_execs
  custom_target(exec,
    build_by_default: true,
    command: ['ln', '-fn', einfo, '@OUTPUT@'],
    output: exec)
endforeach

meson.add_install_script(install_hardlinks,
  rc_bindir,
  'einfo',
  einfo_execs)
