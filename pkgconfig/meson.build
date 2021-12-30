if get_option('pkgconfig')
pkgconfig_dir =	get_option('libdir') / 'pkgconfig'

pkgconfig_conf_data = configuration_data()
if rootprefix != '/'
  pkgconfig_conf_data.set('PREFIX', rootprefix)
else
  pkgconfig_conf_data.set('PREFIX', '')
endif
pkgconfig_conf_data.set('LIB', get_option('libdir'))
pkgconfig_conf_data.set('VERSION', meson.project_version())

pkgconfig_files = [
  'einfo.pc.in',
  'openrc.pc.in',
  ]

foreach file : pkgconfig_files
  configure_file(input : file,
    output : '@BASENAME@',
    configuration : pkgconfig_conf_data,
    install_dir : pkgconfig_dir)
endforeach
endif
