# Copyright (c) 2007-2017 The OpenRC Authors.
# See the Authors file at the top-level directory of this distribution and
# https://github.com/OpenRC/openrc/blob/master/AUTHORS
#
# This file is part of OpenRC. It is subject to the license terms in
# the LICENSE file found in the top-level directory of this
# distribution and at https://github.com/OpenRC/openrc/blob/master/LICENSE
# This file may not be copied, modified, propagated, or distributed
# except according to the terms contained in the LICENSE file.

einfo_pc = configure_file(
  input : 'einfo.pc.in',
  output : 'einfo.pc',
  configuration : substs)

openrc_pc = configure_file(
  input : 'openrc.pc.in',
  output : 'openrc.pc',
  configuration : substs)

install_data(einfo_pc, openrc_pc,
             install_dir : pkgconfiglibdir)
