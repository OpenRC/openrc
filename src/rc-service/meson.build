executable('rc-service',
  ['rc-service.c', misc_c, usage_c, version_h],
  c_args : cc_branding_flags,
  link_with: [libeinfo, librc],
  include_directories: [incdir, einfo_incdir, rc_incdir],
  install: true,
  install_dir: sbindir)
