if get_option('user_services') and get_option('pam') and pam_dep.found()
  shared_library('pam_openrc',
    ['openrc-pam.c'],
    c_args : [cc_branding_flags, cc_user_services_flags],
    dependencies : [pam_dep],
    name_prefix : '',
    link_with : [libeinfo, librc],
    include_directories : [incdir, einfo_incdir, rc_incdir],
    install : true,
    install_dir : join_paths(libdir, 'security'))
endif
