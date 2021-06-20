# Copyright (c) 2007-2017 The OpenRC Authors.
# See the Authors file at the top-level directory of this distribution and
# https://github.com/OpenRC/openrc/blob/master/AUTHORS
#
# This file is part of OpenRC. It is subject to the license terms in
# the LICENSE file found in the top-level directory of this
# distribution and at https://github.com/OpenRC/openrc/blob/master/LICENSE
# This file may not be copied, modified, propagated, or distributed
# except according to the terms contained in the LICENSE file.

conf_d_conf = [
  'bootmisc',
  'fsck',
  'hostname',
  'localmount',
  'netmount',
  'swap',
  'urandom',
]

if mk_net
  conf_d_conf += ['network', 'staticroute']
endif

if os == 'Linux'
  conf_d_conf += [
    'agetty',
    'consolefont',
    'devfs',
    'dmesg',
    'hwclock',
    'keymaps',
    'killprocs',
    'modules',
    'mtab',
    'net-online',
  ]
elif os == 'FreeBSD'
  conf_d_conf += [
    'ipfw',
    'modules',
    'moused',
    'powerd',
    'rarpd',
    'savecore',
    'syscons',
  ]
elif os == 'NetBSD'
  conf_d_conf += [
    'moused',
    'rarpd',
    'savecore',
  ]
endif

install_data(conf_d_conf, install_dir: confdir)
