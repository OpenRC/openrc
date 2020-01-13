man3 = [
  'einfo.3',
  'rc_config.3',
  'rc_deptree.3',
  'rc_find_pids.3',
  'rc_plugin_hook.3',
  'rc_runlevel.3',
  'rc_service.3',
  'rc_stringlist.3',
  ]

man8 = [
  'openrc.8',
  'openrc-run.8',
  'rc-service.8',
  'rc-status.8',
  'rc-update.8',
  'start-stop-daemon.8',
  'supervise-daemon.8',
  ]

if os == 'Linux'
  man8 = man8 + [
    'rc-sstat.8',
    'openrc-init.8',
    'openrc-shutdown.8',
    ]
endif

install_data(man3,
  install_dir : get_option('mandir') / 'man3')
install_data(man8,
  install_dir : get_option('mandir') / 'man8')
meson.add_install_script('meson_man_links.sh', get_option('mandir'), man3 + man8)
