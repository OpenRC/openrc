if get_option('pam') and pam_dep.found()
  shared_module('pam_openrc', 'pam_openrc.c',
    c_args : [cc_branding_flags],
    dependencies : [rc, einfo, shared, pam_dep],
    name_prefix : '',
    include_directories : incdir,
    install : true,
    install_dir : pam_libdir)
endif
