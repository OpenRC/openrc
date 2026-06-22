rc_h_conf_data = configuration_data()
rc_h_conf_data.set('RC_LIBEXECDIR', rc_libexecdir)
rc_h_conf_data.set('RC_PLUGINDIR', pluginsdir)
rc_h_conf_data.set('LOCAL_PREFIX', local_prefix)
rc_h_conf_data.set('SYSCONFDIR', get_option('sysconfdir'))

librc_sources = [
  'librc.c',
  'librc-daemon.c',
  'librc-depend.c',
  'librc-misc.c',
  'librc-stringlist.c',
  'rc-eventlog.c',
]

sysconfdir = get_option('sysconfdir')
rc_path = get_option('rc_path')
if rc_path.length() == 0
  rc_path = [
    sysconfdir / 'rc',
    sysconfdir,
    local_prefix / 'rc',
    local_prefix,
    rc_libexecdir,
  ]
endif
rc_path += '/run/openrc'

rc_h = configure_file(input : 'rc.h.in', output : 'rc.h',
  configuration : rc_h_conf_data)

rc = declare_dependency(
  include_directories: '.',
  sources: rc_h,
  link_with: library('rc', librc_sources,
    c_args: [ '-D_RC_PATH="@0@"'.format('","'.join(rc_path)) ],
    dependencies: kvm_dep,
    include_directories : incdir,
    link_depends : 'rc.map',
    version : '1',
    install : true,
    install_dir : libdir))

install_headers(rc_h)
