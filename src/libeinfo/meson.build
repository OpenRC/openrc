# Copyright (c) 2007-2017 The OpenRC Authors.
# See the Authors file at the top-level directory of this distribution and
# https://github.com/OpenRC/openrc/blob/master/AUTHORS
#
# This file is part of OpenRC. It is subject to the license terms in
# the LICENSE file found in the top-level directory of this
# distribution and at https://github.com/OpenRC/openrc/blob/master/LICENSE
# This file may not be copied, modified, propagated, or distributed
# except according to the terms contained in the LICENSE file.

SHLIB_MAJOR=1

einfo_map_path = join_paths(meson.current_source_dir(), 'einfo.map')

libeinfo = library(
  'einfo',
  'libeinfo.c',
  soversion: SHLIB_MAJOR,
  # version: '1.0.0',
  include_directories : include_directories('../includes'),
  link_args : ['-shared',
               '-Wl,--version-script=' + einfo_map_path],
  c_args: common_compile_args,
  install : true,
)

install_headers('einfo.h')
