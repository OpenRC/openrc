version_h = vcs_tag(input : 'version.h.in', output : 'version.h')
vcs_tag(input : 'version.in', output : 'version')

shared_sources = [
  'misc.c',
  'plugin.c',
  'schedules.c',
  'timeutils.c',
  'rc_exec.c',
  '_usage.c',
  version_h,
]

if os == 'linux'
  shared_sources += 'wtmp.c'
endif

shared_deps = [rc, einfo]

if selinux_dep.found()
  shared_sources += 'selinux.c'
  shared_deps += selinux_dep
endif

shared = declare_dependency(sources: version_h, link_with: static_library('shared', shared_sources, dependencies: shared_deps))
