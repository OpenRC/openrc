init_d_dir = get_option('sysconfdir') / 'init.d'

init_common =  [
  'bootmisc.in',
  'fsck.in',
  'hostname.in',
  'local.in',
  'localmount.in',
  'loopback.in',
  'netmount.in',
  'osclock.in',
  'root.in',
  'savecache.in',
  'swap.in',
  'swclock.in',
  'sysctl.in',
  'runsvdir.in',
  's6-svscan.in',
  'user.in',
  ]

if get_option('newnet')
  init_common = init_common + [
    'network.in',
    'staticroute.in',
    ]
endif

init_Linux =  [
  'agetty.in',
  'binfmt.in',
  'cgroups.in',
  'consolefont.in',
  'devfs.in',
  'dmesg.in',
  'hwclock.in',
  'keymaps.in',
  'killprocs.in',
  'modules.in',
  'mount-ro.in',
  'mtab.in',
  'numlock.in',
  'procfs.in',
  'net-online.in',
  'save-keymaps.in',
  'save-termencoding.in',
  'seedrng.in',
  'sysfs.in',
  'termencoding.in',
  ]

init_BSD =  [
  'hostid.in',
  'moused.in',
  'newsyslog.in',
  'pf.in',
  'rarpd.in',
  'rc-enabled.in',
  'rpcbind.in',
  'savecore.in',
  'syslogd.in',
  'urandom.in',
  ]

init_FreeBSD =  [
  'adjkerntz.in',
  'devd.in',
  'dumpon.in',
  'encswap.in',
  'ipfw.in',
  'modules.in',
  'mixer.in',
  'nscd.in',
  'powerd.in',
  'syscons.in',
  ]

init_NetBSD =  [
  'devdb.in',
  'swap-blk.in',
  'ttys.in',
  'wscons.in',
  ]

init_data = init_common
if os == 'Dragonfly'
  init_data = init_data + init_BSD
elif os == 'FreeBSD' or os == 'Gnu-kFreeBSD'
  init_data = init_data + init_BSD + init_FreeBSD
elif os == 'Linux'
  init_data = init_data + init_Linux
elif os == 'NetBSD'
  init_data = init_data + init_BSD + init_NetBSD
  endif

foreach init_d_file : init_data
  configure_file(input : init_d_file,
    output : '@BASENAME@',
    configuration : init_d_conf_data,
    install_dir: init_d_dir,
    install_mode: 'rwxr-xr-x')
endforeach
install_symlink('functions.sh',
  pointing_to: rc_shdir / 'functions.sh',
  install_dir: init_d_dir)

if get_option('sysvinit') == true and os == 'Linux'
  foreach x : get_option('agetty')
    link_name = 'agetty.' + x
    install_symlink(link_name,
      install_dir: init_d_dir,
      pointing_to: 'agetty')
  endforeach
endif
