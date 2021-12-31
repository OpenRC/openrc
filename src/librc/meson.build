rc_h_conf_data = configuration_data()
if rootprefix == '/'
  rc_h_conf_data.set('PREFIX', '')
  else
  rc_h_conf_data.set('PREFIX', rootprefix)
	endif
rc_h_conf_data.set('LIB', libname)
rc_h_conf_data.set('LIBEXECDIR', rc_libexecdir)
rc_h_conf_data.set('LOCAL_PREFIX', local_prefix)
rc_h_conf_data.set('PKG_PREFIX', pkg_prefix)
rc_h_conf_data.set('SYSCONFDIR', get_option('sysconfdir'))

librc_version = '1'

librc_sources = [
  'librc.c',
  'librc-daemon.c',
  'librc-depend.c',
  'librc-misc.c',
  'librc-stringlist.c',
  ]

rc_h = configure_file(input : 'rc.h.in', output : 'rc.h',
  configuration : rc_h_conf_data)

librc = library('rc', librc_sources,
  dependencies: kvm_dep,
  include_directories : [incdir, einfo_incdir],
  link_depends : 'rc.map',
  version : librc_version,
  install : true,
  install_dir : libdir)

install_headers(rc_h)
