if get_option('pam') and pam_dep.found()
  shared_module('pam_openrc', 'pam_openrc.c',
    c_args : [cc_branding_flags],
    dependencies : [einfo, shared, pam_dep],
    name_prefix : '',
    link_with : librc,
    include_directories : [incdir, rc_incdir],
    install : true,
    install_dir : pam_libdir)
endif
