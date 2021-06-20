# Copyright (c) 2007-2017 The OpenRC Authors.
# See the Authors file at the top-level directory of this distribution and
# https://github.com/OpenRC/openrc/blob/master/AUTHORS
#
# This file is part of OpenRC. It is subject to the license terms in
# the LICENSE file found in the top-level directory of this
# distribution and at https://github.com/OpenRC/openrc/blob/master/LICENSE
# This file may not be copied, modified, propagated, or distributed
# except according to the terms contained in the LICENSE file.

etc_rc = configure_file(
  input : 'rc.in',
  output : 'rc',
  configuration : substs,
)


etc_rc_shutdown = configure_file(
  input : 'rc.shutdown.in',
  output : 'rc.shutdown',
  configuration : substs,
)

etc_conf = [
  'rc.conf'
]
etc_bin = []

if os == 'FreeBSD'
  etc_bin += [etc_rc, etc_rc_shutdown, 'rc.devd']
  etc_conf += ['devd.conf']
elif os == 'Linux'
  etc_bin += []
  etc_conf += []
elif os == 'NetBSD'
  etc_bin += [etc_rc, etc_rc_shutdown]
  etc_conf += []
endif

install_data(etc_bin, etc_conf, install_dir: sysconfdir)

