executable('openrc',
  ['rc.c', 'rc-logger.c', misc_c, plugin_c, rc_exec_c, usage_c, version_h],
  c_args : cc_branding_flags,
  link_with: [libeinfo, librc],
  dependencies: [dl_dep, util_dep],
  include_directories: [incdir, einfo_incdir, rc_incdir],
  install: true,
  install_dir: sbindir)
