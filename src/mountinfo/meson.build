libmountinfo = library('mountinfo',
  ['libmountinfo.c', misc_c, usage_c, version_h],
  c_args : cc_branding_flags,
  include_directories: [incdir, einfo_incdir, rc_incdir],
  link_with: [libeinfo, librc],
  install: true,
  install_dir: libdir)

executable('mountinfo',
  ['mountinfo.c', misc_c, usage_c, version_h],
  c_args : cc_branding_flags,
  include_directories: [incdir, einfo_incdir, rc_incdir, mountinfo_incdir],
  link_with: [libeinfo, librc, libmountinfo],
  install: true,
  install_dir: rc_bindir)
