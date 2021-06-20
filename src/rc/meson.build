# Copyright (c) 2007-2017 The OpenRC Authors.
# See the Authors file at the top-level directory of this distribution and
# https://github.com/OpenRC/openrc/blob/master/AUTHORS
#
# This file is part of OpenRC. It is subject to the license terms in
# the LICENSE file found in the top-level directory of this
# distribution and at https://github.com/OpenRC/openrc/blob/master/LICENSE
# This file may not be copied, modified, propagated, or distributed
# except according to the terms contained in the LICENSE file.

cdata = configuration_data()

if git_head_short != ''
  version = meson.project_version() + '.' + git_head_short
else
  version = meson.project_version()
endif
cdata.set_quoted('VERSION', version)
branding = get_option('branding')
if branding != ''
  cdata.set_quoted('BRANDING', branding)
endif

configure_file(
  output: 'version.h',
  configuration: cdata,
  install_dir: get_option('includedir'),
)

linkdir = libexecdir

prog_link_args = []

if prefix == '/'
  # Some systems don't include /lib in their standard link path so we should embed
  # it if different. This is currently hardcoded for NetBSD which has two dynamic
  # linkers and we need to use the one in /libexec instead of /usr/libexec
  if run_command('test', '-e', '/libexec/ld.elf_so').returncode() == 0
    prog_link_args += ['-Wl,-dynamic-linker=/libexec/ld.elf_so']
  endif
endif

rc_sources = [
  '_usage.c',
  'rc-misc.c',
  'rc-schedules.c',
  'rc-plugin.c',
  'rc-wtmp.c',
  'rc-logger.c'
]

if mkselinux
  rc_sources += ['rc-selinux.c']
endif

rc_static = static_library(
  'rc_static',
  rc_sources,
  include_directories : openrc_includes,
  c_args: common_compile_args,
  install : false,
)

checkpath = executable(
  'checkpath',
  'checkpath.c',
  c_args: common_compile_args,
  include_directories : openrc_includes,
  link_with: [rc_static, librc, libeinfo],
  link_args: prog_link_args,
  install : true,
  install_dir: bindir,
)

kill_all = executable(
  'kill_all',
  'kill_all.c',
  c_args: common_compile_args,
  include_directories : openrc_includes,
  link_with: [rc_static, librc, libeinfo],
  link_args: prog_link_args,
  install : true,
  install_dir: bindir,
)

foreach prog : [
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
  executable(
    prog,
    'do_e.c',
    c_args: common_compile_args,
    include_directories : openrc_includes,
    link_with: [rc_static, librc, libeinfo],
    link_args: prog_link_args,
    install : true,
    install_dir: join_paths(linkdir, 'bin'),
  )
endforeach

fstabinfo = executable(
  'fstabinfo',
  'fstabinfo.c',
  c_args: common_compile_args,
  include_directories : openrc_includes,
  link_with: [rc_static, librc, libeinfo],
  link_args: prog_link_args,
  install : true,
  install_dir: bindir,
)

openrc_init = executable(
  'openrc-init',
  'openrc-init.c',
  c_args: common_compile_args,
  include_directories : openrc_includes,
  link_with: [rc_static, librc, libeinfo],
  link_args: prog_link_args,
  install : true,
  install_dir: bindir,
)

is_newer_than = executable(
  'is_newer_than',
  'is_newer_than.c',
  c_args: common_compile_args,
  include_directories : openrc_includes,
  link_with: [rc_static, librc, libeinfo],
  link_args: prog_link_args,
  install : true,
  install_dir: bindir,
)

is_older_than = executable(
  'is_older_than',
  'is_older_than.c',
  c_args: common_compile_args,
  include_directories : openrc_includes,
  link_with: [rc_static, librc, libeinfo],
  link_args: prog_link_args,
  install : true,
  install_dir: bindir,
)

foreach prog : [
  'mark_service_starting',
  'mark_service_started',
  'mark_service_stopping',
  'mark_service_stopped',
  'mark_service_inactive',
  'mark_service_wasinactive',
  'mark_service_hotplugged',
  'mark_service_failed'
]
  executable(
    prog,
    'do_mark_service.c',
    c_args: common_compile_args,
    include_directories : openrc_includes,
    link_with: [rc_static, librc, libeinfo],
    link_args: prog_link_args,
    install : true,
    install_dir: join_paths(linkdir, 'sbin'),
  )
endforeach

mountinfo = executable(
  'mountinfo',
  'mountinfo.c',
  c_args: common_compile_args,
  include_directories : openrc_includes,
  link_with: [rc_static, librc, libeinfo],
  link_args: prog_link_args,
  install : true,
  install_dir: bindir,
)

openrc = executable(
  'openrc',
  'rc.c',
  c_args: common_compile_args,
  include_directories : openrc_includes,
  link_with: [rc_static, librc, libeinfo],
  dependencies: [library_deps],
  link_args: prog_link_args,
  install : true,
  install_dir: sbindir,
)

rc = executable(
  'rc',
  'rc.c',
  c_args: common_compile_args,
  include_directories : openrc_includes,
  link_with: [rc_static, librc, libeinfo],
  dependencies: [library_deps],
  link_args: prog_link_args,
  install : true,
  install_dir: sbindir,
)

openrc_shutdown = executable(
  'openrc-shutdown',
  'openrc-shutdown.c',
  c_args: common_compile_args,
  include_directories : openrc_includes,
  link_with: [rc_static, librc, libeinfo],
  link_args: prog_link_args,
  install : true,
  install_dir: bindir,
)

openrc_run = executable(
  'openrc-run',
  'openrc-run.c',
  c_args: common_compile_args,
  include_directories : openrc_includes,
  link_with: [rc_static, librc, libeinfo],
  dependencies: [library_deps],
  link_args: prog_link_args,
  install : true,
  install_dir: sbindir,
)

runscript = executable(
  'runscript',
  'openrc-run.c',
  c_args: common_compile_args,
  include_directories : openrc_includes,
  link_with: [rc_static, librc, libeinfo],
  dependencies: [library_deps],
  link_args: prog_link_args,
  install : true,
  install_dir: sbindir,
)

rc_abort = executable(
  'rc-abort',
  'rc-abort.c',
  c_args: common_compile_args,
  include_directories : openrc_includes,
  link_with: [rc_static, librc, libeinfo],
  link_args: prog_link_args,
  install : true,
  install_dir: join_paths(linkdir, 'sbin'),
)

rc_depend = executable(
  'rc-depend',
  'rc-depend.c',
  c_args: common_compile_args,
  include_directories : openrc_includes,
  link_with: [rc_static, librc, libeinfo],
  link_args: prog_link_args,
  install : true,
  install_dir: bindir,
)

rc_status = executable(
  'rc-status',
  'rc-status.c',
  c_args: common_compile_args,
  include_directories : openrc_includes,
  link_with: [rc_static, librc, libeinfo],
  link_args: prog_link_args,
  install : true,
  install_dir: bindir,
)

rc_service = executable(
  'rc-service',
  'rc-service.c',
  c_args: common_compile_args,
  include_directories : openrc_includes,
  link_with: [rc_static, librc, libeinfo],
  link_args: prog_link_args,
  install : true,
  install_dir: sbindir,
)

rc_update = executable(
  'rc-update',
  'rc-update.c',
  c_args: common_compile_args,
  include_directories : openrc_includes,
  link_with: [rc_static, librc, libeinfo],
  link_args: prog_link_args,
  install : true,
  install_dir: sbindir,
)

start_stop_daemon = executable(
  'start-stop-daemon',
  'start-stop-daemon.c',
  c_args: common_compile_args,
  include_directories : openrc_includes,
  link_with: [rc_static, librc, libeinfo],
  link_args: prog_link_args,
  install : true,
  install_dir: sbindir,
)

supervise_daemon = executable(
  'supervise-daemon',
  'supervise-daemon.c',
  c_args: common_compile_args,
  include_directories : openrc_includes,
  link_with: [rc_static, librc, libeinfo],
  link_args: prog_link_args,
  install : true,
  install_dir: sbindir,
)

foreach prog : [
  'service_get_value',
  'service_set_value',
  'get_options',
  'save_options',
]
  executable(
    prog,
    'do_value.c',
    c_args: common_compile_args,
    include_directories : openrc_includes,
    link_with: [rc_static, librc, libeinfo],
    link_args: prog_link_args,
    install : true,
    install_dir: join_paths(linkdir, 'sbin'),
  )
endforeach

foreach prog : [
  'service_starting',
  'service_started',
  'service_stopping',
  'service_stopped',
  'service_inactive',
  'service_wasinactive',
  'service_hotplugged',
  'service_started_daemon',
  'service_crashed:',
  'do_service.o',
  'rc-misc.o',
]
  executable(
    prog,
    'do_service.c',
    c_args: common_compile_args,
    include_directories : openrc_includes,
    link_with: [rc_static, librc, libeinfo],
    link_args: prog_link_args,
    install : true,
    install_dir: join_paths(linkdir, 'sbin'),
  )
endforeach

shell_var = executable(
  'shell_var',
  'shell_var.c',
  c_args: common_compile_args,
  include_directories : openrc_includes,
  link_with: [rc_static, librc, libeinfo],
  link_args: prog_link_args,
  install : true,
  install_dir: bindir,
)

swclock = executable(
  'swclock',
  'swclock.c',
  c_args: common_compile_args,
  include_directories : openrc_includes,
  link_with: [rc_static, librc, libeinfo],
  link_args: prog_link_args,
  install : true,
  install_dir: join_paths(linkdir, 'sbin'),
)
