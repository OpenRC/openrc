# Copyright (c) 2007-2017 The OpenRC Authors.
# See the Authors file at the top-level directory of this distribution and
# https://github.com/OpenRC/openrc/blob/master/AUTHORS
#
# This file is part of OpenRC. It is subject to the license terms in
# the LICENSE file found in the top-level directory of this
# distribution and at https://github.com/OpenRC/openrc/blob/master/LICENSE
# This file may not be copied, modified, propagated, or distributed
# except according to the terms contained in the LICENSE file.

sh_install_dir = join_paths(libexecdir, 'sh')

sh_incs = [
  'rc-mount.sh',
  # 'functions.sh',
  # 'rc-functions.sh',
  'runit.sh',
  's6.sh',
  'start-stop-daemon.sh',
  'supervise-daemon.sh'
]

sh_bins = [
  ['gendepends.sh.in', 'gendepends.sh'],
  ['init.sh' + sfx, 'init.sh'],
  ['openrc-run.sh.in', 'openrc-run.sh'],
]

if os == 'FreeBSD'
# sh_bins += []
elif os == 'Linux'
  sh_bins += [
    ['binfmt.sh.in', 'binfmt.sh'],
    ['cgroup-release-agent.sh.in', 'cgroup-release-agent.sh'],
    ['init-early.sh' + sfx, 'init-early.sh'],
    ['migrate-to-run.sh.in', 'migrate-to-run.sh'],
    ['rc-cgroup.sh.in', 'rc-cgroup.sh'],
  ]
elif os == 'NetBSD'
  # sh_bins += []
endif

install_data(sh_incs,
             install_dir: sh_install_dir,
             install_mode: incmode,
            )

foreach arg : ['functions.sh', 'rc-functions.sh']
  configure_file(input: arg + '.in',
                 output: arg,
                 configuration : substs,
                 install_dir: sh_install_dir,
                )
endforeach



foreach arg : sh_bins
  sh_bin = configure_file(input: arg.get(0),
                          output: arg.get(1),
                          configuration : substs)
  install_data(sh_bin,
               install_dir: sh_install_dir,
               install_mode: binmode,
              )
endforeach

# TODO : update test scripts, make it can run in build directory
sh_check_cmd = 'cd @0@; ./runtests.sh'.format(
  meson.current_source_dir(),
)

run_target(
  'sh-check',
  command : ['sh', '-c', sh_check_cmd],
)

