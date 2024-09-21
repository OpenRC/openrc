if get_option('pam') and pam_dep.found()
  shared_module('pam_openrc',
    ['openrc-pam.c', misc_c, version_h],
    c_args : [cc_branding_flags],
    dependencies : [pam_dep],
    name_prefix : '',
    link_with : [libeinfo, librc],
    include_directories : [incdir, einfo_incdir, rc_incdir],
    install : true,
    install_dir : libdir / 'security')
endif
