misc_c = files([
  'misc.c',
  ])

plugin_c = files([
  'plugin.c',
  ])

schedules_c = files([
  'schedules.c',
  ])

timeutils_c = files([
  'timeutils.c'
  ])

rc_exec_c = files([
  'rc_exec.c',
  ])

if selinux_dep.found()
  selinux_c = files([
    'selinux.c',
    ])
else
  selinux_c = []
endif

wtmp_c = files([
  'wtmp.c',
  ])

usage_c = files([
  '_usage.c',
  ])

version_h = vcs_tag(
  input : 'version.h.in',
  output : 'version.h')

version_f = vcs_tag(
  input : 'version.in',
  output : 'version')
