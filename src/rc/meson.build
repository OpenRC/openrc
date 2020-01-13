version_h_conf_data = configuration_data()
version_h_conf_data.set_quoted('VERSION', version_data)
if get_option('branding') != ''
  version_h_conf_data.set_quoted('BRANDING', get_option('branding'))
endif

broadcast_sources = files([
  'broadcast.c',
  ])

misc_sources = files([
  'rc-misc.c',
  ])

pipe_sources = files([
  'rc-pipes.c',
  ])

plugin_sources = ([
  'rc-plugin.c',
  ])

schedule_sources = files([
  'rc-schedules.c',
  ])

usage_sources = files([
  '_usage.c',
  ])


if get_option('selinux').enabled()
  selinux_sources = files([
    'rc-selinux.c',
    ])
else
  selinux_sources = []
endif

wtmp_sources = files([
  'rc-wtmp.c',
  ])

sysvinit_sources = files([
  'rc-sysvinit.c',
  ])

einfo_execs = [
  'einfon',
  'einfo',
  'ewarnn',
  'ewarn',
  'eerrorn',
  'eerror',
  'ebegin',
  'eend',
  'ewend',
  'eindent',
  'eoutdent',
  'esyslog',
  'eval_ecolors',
  'ewaitfile',
  'veinfo',
  'vewarn',
  'vebegin',
  'veend',
  'vewend',
  'veindent',
  'veoutdent',
  ]

value_execs = [
  'service_get_value',
  'service_set_value',
  'get_options',
  'save_options',
  ]

service_execs = [
  'service_starting',
  'service_started',
  'service_stopping',
  'service_stopped',
  'service_inactive',
  'service_wasinactive',
  'service_hotplugged',
  'service_started_daemon',
  'service_crashed',
  ]

mark_service_execs =	[
  'mark_service_starting',
  'mark_service_started',
  'mark_service_stopping',
  'mark_service_stopped',
  'mark_service_inactive',
  'mark_service_wasinactive',
  'mark_service_hotplugged',
  'mark_service_failed',
  'mark_service_crashed',
  ]

rc_bindir = rc_libexecdir / 'bin'
rc_sbindir = rc_libexecdir / 'sbin'

configure_file(output: 'version.h', configuration: version_h_conf_data)

executable('rc-status', ['rc-status.c', misc_sources, usage_sources],
  link_with: [libeinfo, librc],
  dependencies: [util_dep],
  include_directories: [incdir, einfo_incdir, rc_incdir],
  install: true,
  install_dir: bindir)

executable('openrc',
  ['rc.c', 'rc-logger.c', misc_sources, plugin_sources, usage_sources],
  link_with: [libeinfo, librc],
  dependencies: [dl_dep, util_dep],
  include_directories: [incdir, einfo_incdir, rc_incdir],
  install: true,
  install_dir: sbindir)

executable('openrc-run',
  ['openrc-run.c', misc_sources, plugin_sources, usage_sources, 
  selinux_sources],
  c_args : [cc_audit_flags, cc_pam_flags, cc_selinux_flags],
  link_with: [libeinfo, librc],
  dependencies: [dl_dep, libpam, selinux_dep, util_dep],
  include_directories: [incdir, einfo_incdir, rc_incdir],
  install: true,
  install_dir: sbindir)

executable('rc',
  ['rc.c', 'rc-logger.c', misc_sources, plugin_sources, usage_sources],
  link_with: [libeinfo, librc],
  dependencies: [dl_dep, util_dep],
  include_directories: [incdir, einfo_incdir, rc_incdir],
  install: true,
  install_dir: sbindir)

executable('rc-service',
  ['rc-service.c', misc_sources, usage_sources],
  link_with: [libeinfo, librc],
  include_directories: [incdir, einfo_incdir, rc_incdir],
  install: true,
  install_dir: sbindir)

executable('rc-update',
  ['rc-update.c', misc_sources, usage_sources],
  link_with: [libeinfo, librc],
  include_directories: [incdir, einfo_incdir, rc_incdir],
  install: true,
  install_dir: sbindir)

executable('runscript',
  ['openrc-run.c', misc_sources, usage_sources, 'rc-plugin.c',
    selinux_sources],
  c_args : [cc_audit_flags, cc_pam_flags, cc_selinux_flags],
  link_with: [libeinfo, librc],
  dependencies: [dl_dep, libpam, util_dep, selinux_dep],
  include_directories: [incdir, einfo_incdir, rc_incdir],
  install: true,
  install_dir: sbindir)

executable('start-stop-daemon',
  ['start-stop-daemon.c', misc_sources, pipe_sources, schedule_sources,
	selinux_sources, usage_sources],
  c_args : [cc_audit_flags, cc_pam_flags, cc_selinux_flags],
  link_with: [libeinfo, librc],
  dependencies: [dl_dep, libpam, util_dep, selinux_dep],
  include_directories: [incdir, einfo_incdir, rc_incdir],
  install: true,
  install_dir: sbindir)

executable('supervise-daemon',
  ['supervise-daemon.c', misc_sources, plugin_sources, schedule_sources,
    usage_sources],
  c_args : [cc_pam_flags, cc_selinux_flags],
  link_with: [libeinfo, librc],
  dependencies: [dl_dep, libpam, util_dep, selinux_dep],
  include_directories: [incdir, einfo_incdir, rc_incdir],
  install: true,
  install_dir: sbindir)

if os == 'Linux'
  executable('openrc-init',
    ['openrc-init.c', plugin_sources, wtmp_sources],
    include_directories: [incdir, einfo_incdir, rc_incdir],
    link_with: [libeinfo, librc],
    dependencies: [dl_dep],
    install: true,
    install_dir: sbindir)

  executable('openrc-shutdown',
    ['openrc-shutdown.c', broadcast_sources, misc_sources,
		usage_sources, wtmp_sources, sysvinit_sources],
    include_directories: [incdir, einfo_incdir, rc_incdir],
    link_with: [libeinfo, librc],
    install: true,
    install_dir: sbindir)
endif

foreach exec: einfo_execs
  executable(exec,
    ['do_e.c', misc_sources],
    include_directories: [incdir, einfo_incdir, rc_incdir],
    link_with: [libeinfo, librc],
    install: true,
    install_dir: rc_bindir)
endforeach

executable('checkpath',
  ['checkpath.c', misc_sources, usage_sources, selinux_sources],
  c_args : [cc_audit_flags, cc_pam_flags, cc_selinux_flags],
  include_directories: [incdir, einfo_incdir, rc_incdir],
  link_with: [libeinfo, librc],
  dependencies: [libpam, selinux_dep],
  install: true,
  install_dir: rc_bindir)

executable('fstabinfo',
  ['fstabinfo.c', misc_sources, usage_sources],
  include_directories: [incdir, einfo_incdir, rc_incdir],
  link_with: [libeinfo, librc],
  install: true,
  install_dir: rc_bindir)

executable('mountinfo',
  ['mountinfo.c', misc_sources, usage_sources],
  include_directories: [incdir, einfo_incdir, rc_incdir],
  link_with: [libeinfo, librc],
  install: true,
  install_dir: rc_bindir)

executable('rc-depend',
  ['rc-depend.c', misc_sources, usage_sources],
  include_directories: [incdir, einfo_incdir, rc_incdir],
  link_with: [libeinfo, librc],
  install: true,
  install_dir: rc_bindir)

executable('is_newer_than',
  ['is_newer_than.c', misc_sources],
  include_directories: [incdir, einfo_incdir, rc_incdir],
  link_with: [libeinfo, librc],
  install: true,
  install_dir: rc_bindir)

executable('is_older_than',
  ['is_older_than.c', misc_sources],
  include_directories: [incdir, einfo_incdir, rc_incdir],
  link_with: [libeinfo, librc],
  install: true,
  install_dir: rc_bindir)

foreach exec : service_execs
  executable(exec,
    ['do_service.c', misc_sources],
    include_directories: [incdir, einfo_incdir, rc_incdir],
    link_with: [libeinfo, librc],
    install: true,
    install_dir: rc_bindir)
endforeach

foreach exec : value_execs
  executable(exec,
    ['do_value.c', misc_sources],
    include_directories: [incdir, einfo_incdir, rc_incdir],
    link_with: [libeinfo, librc],
    install: true,
    install_dir: rc_bindir)
endforeach

if os == 'Linux'
  executable('kill_all',
	  ['kill_all.c', usage_sources],
    include_directories: [incdir, einfo_incdir, rc_incdir],
    link_with: [libeinfo,librc],
    install: true,
    install_dir: rc_bindir)
  endif

executable('shell_var',
  ['shell_var.c'],
  install: true,
  install_dir: rc_bindir)

foreach exec : mark_service_execs
  executable(exec,
    ['do_mark_service.c', misc_sources],
    include_directories: [incdir, einfo_incdir, rc_incdir],
    link_with: [libeinfo,librc],
    install: true,
    install_dir: rc_sbindir)
endforeach

executable('rc-abort',
  'rc-abort.c',
  include_directories: [einfo_incdir],
  link_with: [libeinfo],
  install: true,
  install_dir: rc_sbindir)

executable('swclock',
  ['swclock.c', misc_sources, usage_sources],
    include_directories: [incdir, einfo_incdir, rc_incdir],
    link_with: [libeinfo,librc],
  install: true,
  install_dir: rc_sbindir)
