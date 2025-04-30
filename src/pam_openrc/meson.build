if get_option('pam') and pam_dep.found()
  shared_module('pam_openrc', 'pam_openrc.c',
    c_args : [cc_branding_flags],
    dependencies : [shared, pam_dep],
    name_prefix : '',
    link_with : [libeinfo, librc],
    include_directories : [incdir, einfo_incdir, rc_incdir],
    install : true,
    install_dir : pam_libdir)
endif
