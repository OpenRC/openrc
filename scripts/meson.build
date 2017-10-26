# Copyright (c) 2007-2017 The OpenRC Authors.
# See the Authors file at the top-level directory of this distribution and
# https://github.com/OpenRC/openrc/blob/master/AUTHORS
#
# This file is part of OpenRC. It is subject to the license terms in
# the LICENSE file found in the top-level directory of this
# distribution and at https://github.com/OpenRC/openrc/blob/master/LICENSE
# This file may not be copied, modified, propagated, or distributed
# except according to the terms contained in the LICENSE file.

scripts_install_dir = join_paths(libexecdir, 'bin')
install_data('on_ac_power',
             install_dir: scripts_install_dir,
             install_mode: binmode,
            )

scripts_src = []

if os == 'Linux'
  scripts_src += ['rc-sstat']

  if mk_sysvinit
    scripts_src += ['halt',
                    'poweroff',
                    'reboot',
                    'shutdown']
  endif
endif

foreach arg : scripts_src
  script_bin = configure_file(input: arg + '.in',
                              output: arg,
                              configuration : substs)
  install_data(script_bin,
               install_dir: scripts_install_dir,
               install_mode: binmode,
              )
endforeach

if os == 'Linux'
  foreach name : scripts_src
    src = join_paths(libexecdir, name)
    dst = join_paths(sbindir, name)
    cmd = 'mkdir -p $DESTDIR@0@; ln -sf @1@ $DESTDIR@2@'.format(sbindir, src, dst)
    meson.add_install_script('sh', '-c', cmd)
  endforeach
endif
