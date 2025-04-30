rc_h_conf_data = configuration_data()
rc_h_conf_data.set('RC_LIBEXECDIR', rc_libexecdir)
rc_h_conf_data.set('RC_PLUGINDIR', pluginsdir)
rc_h_conf_data.set('LOCAL_PREFIX', local_prefix)
rc_h_conf_data.set('PKG_PREFIX', pkg_prefix)
rc_h_conf_data.set('SYSCONFDIR', get_option('sysconfdir'))

librc_sources = [
  'librc.c',
  'librc-daemon.c',
  'librc-depend.c',
  'librc-misc.c',
  'librc-stringlist.c',
]

rc_h = configure_file(input : 'rc.h.in', output : 'rc.h',
  configuration : rc_h_conf_data)

rc = declare_dependency(
  include_directories: '.',
  sources: rc_h,
  link_with: library('rc', librc_sources,
    dependencies: kvm_dep,
    include_directories : incdir,
    link_depends : 'rc.map',
    version : '1',
    install : true,
    install_dir : libdir))

install_headers(rc_h)
