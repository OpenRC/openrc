# Copyright (c) 2007-2017 The OpenRC Authors.
# See the Authors file at the top-level directory of this distribution and
# https://github.com/OpenRC/openrc/blob/master/AUTHORS
#
# This file is part of OpenRC. It is subject to the license terms in
# the LICENSE file found in the top-level directory of this
# distribution and at https://github.com/OpenRC/openrc/blob/master/LICENSE
# This file may not be copied, modified, propagated, or distributed
# except according to the terms contained in the LICENSE file.

man3= [
  'einfo',
  'rc_config',
  'rc_deptree',
  'rc_find_pids',
  'rc_plugin_hook',
  'rc_runlevel',
  'rc_service',
  'rc_stringlist'
]

man8= [
  'rc-service',
  'rc-status',
  'rc-update',
  'openrc',
  'openrc-run',
  'start-stop-daemon',
  'supervise-daemon'
]

if os == 'Linux'
  man8 += [
    'rc-sstat',
    'openrc-init',
    'openrc-shutdown'
  ]
endif

foreach name : man3
  man = name + '.3'
  install_man(man, install_dir: join_paths(mandir, 'man3'))
  sed_args = ['-e', 's/ ,//g', '-n', '-e', '/^\.Sh NAME$/,/\.Sh/  s/\.Nm //p']
  manaliases = run_command('sed', sed_args, man).stdout().split()
  foreach alias : manaliases
    if alias != name
      dst = join_paths(mandir, 'man3', alias + '.3.gz')
      cmd = 'ln -sf @0@ $DESTDIR@1@'.format(man + '.gz', dst)
      meson.add_install_script('sh', '-c', cmd)
    endif
  endforeach
endforeach

foreach name : man8
  man = name + '.8'
  install_man(man, install_dir: join_paths(mandir, 'man8'))
  sed_args = ['-e', 's/ ,//g', '-n', '-e', '/^\.Sh NAME$/,/\.Sh/  s/\.Nm //p']
  manaliases = run_command('sed', sed_args, man).stdout().split()
  foreach alias : manaliases
    if alias != name
      dst = join_paths(mandir, 'man8', alias + '.8.gz')
      cmd = 'ln -sf @0@ $DESTDIR@1@'.format(man + '.gz', dst)
      meson.add_install_script('sh', '-c', cmd)
    endif
  endforeach
endforeach
