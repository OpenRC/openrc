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

if os == 'Linux'
  shared_sources += 'wtmp.c'
endif

shared_deps = [einfo]

if selinux_dep.found()
  shared_sources += 'selinux.c'
  shared_deps += selinux_dep
endif

shared = declare_dependency(sources: version_h,
  link_with: static_library('shared', shared_sources,
    c_args: [cc_pam_flags, cc_selinux_flags],
    dependencies: shared_deps,
    link_with: librc,
    include_directories: rc_incdir))
