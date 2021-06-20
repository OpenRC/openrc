# Copyright (c) 2007-2017 The OpenRC Authors.
# See the Authors file at the top-level directory of this distribution and
# https://github.com/OpenRC/openrc/blob/master/AUTHORS
#
# This file is part of OpenRC. It is subject to the license terms in
# the LICENSE file found in the top-level directory of this
# distribution and at https://github.com/OpenRC/openrc/blob/master/LICENSE
# This file may not be copied, modified, propagated, or distributed
# except according to the terms contained in the LICENSE file.

init_d_srcs = [
  'bootmisc',
  'fsck',
  'hostname',
  'local',
  'localmount',
  'loopback',
  'netmount',
  'osclock',
  'root',
  'savecache',
  'swap',
  'swclock',
  'sysctl',
  'runsvdir',
  'urandom',
  's6-svscan',
]

# Are we installing our network scripts?
if mk_net
  init_d_srcs += [
    'network',
    'staticroute',
  ]
endif

if os == 'FreeBSD'
  # Generic BSD scripts
  init_d_srcs += [
    'hostid',
    'modules',
    'moused',
    'newsyslog',
    'pf',
    'rarpd',
    'rc-enabled',
    'rpcbind',
    'savecore',
    'syslogd'
  ]
  # These are FreeBSD specific
  init_d_srcs += [
    'adjkerntz',
    'devd',
    'dumpon',
    'encswap',
    'ipfw modules-load',
    'mixer',
    'nscd',
    'powerd',
    'syscons'
  ]
elif os == 'Linux'
  init_d_srcs += [
    'agetty',
    'binfmt',
    'devfs',
    'dmesg',
    'hwclock',
    'consolefont',
    'keymaps',
    'killprocs',
    'modules',
    'modules-load',
    'mount-ro',
    'mtab',
    'numlock',
    'procfs',
    'net-online',
    'sysfs',
    'termencoding'
  ]
elif os == 'NetBSD'
  # Generic BSD scripts
  init_d_srcs += [
    'hostid',
    'moused',
    'newsyslog',
    'pf',
    'rarpd',
    'rc-enabled',
    'rpcbind',
    'savecore',
    'syslogd'
  ]
  # These are NetBSD specific
  init_d_srcs += [
    'devdb',
    'swap-blk',
    'ttys',
    'wscons'
  ]
endif

foreach arg : init_d_srcs
  configure_file(input: arg + '.in',
                 output: arg,
                 install : true,
                 install_dir: initdir,
                 configuration : substs)
endforeach
