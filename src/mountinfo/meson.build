foreach exec : ['mountinfo', 'rc_unmount']
  executable(exec,
    ['mountinfo.c', rc_exec_c, timeutils_c, misc_c, usage_c, version_h],
    c_args : cc_branding_flags,
    include_directories: [incdir, einfo_incdir, rc_incdir],
    link_with: [libeinfo, librc],
    install: true,
    install_dir: rc_bindir)
endforeach
