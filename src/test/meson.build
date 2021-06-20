# Copyright (c) 2007-2017 The OpenRC Authors.
# See the Authors file at the top-level directory of this distribution and
# https://github.com/OpenRC/openrc/blob/master/AUTHORS
#
# This file is part of OpenRC. It is subject to the license terms in
# the LICENSE file found in the top-level directory of this
# distribution and at https://github.com/OpenRC/openrc/blob/master/LICENSE
# This file may not be copied, modified, propagated, or distributed
# except according to the terms contained in the LICENSE file.

# TODO : update test scripts, make it can run in build directory
check_cmd = 'cd @0@; ./runtests.sh'.format(
  meson.current_source_dir(),
)

verbose_check_cmd = 'cd @0@; VERBOSE=yes ./runtests.sh'.format(
  meson.current_source_dir(),
)

check_clean_cmd = 'cd @0@; rm -rf *.out tmp-*'.format(
  meson.current_source_dir(),
)

run_target(
  'check',
  command : ['sh', '-c', check_cmd],
)

run_target(
  'verbose-check',
  command : ['sh', '-c', verbose_check_cmd],
)

run_target(
  'check-clean',
  command : ['sh', '-c', check_clean_cmd],
)
