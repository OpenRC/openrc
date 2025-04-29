runlevel_dir =get_option('sysconfdir') / 'runlevels'

sysinit = []
sysinit_linux = [
  'cgroups',
  'devfs',
  'dmesg',
  'sysfs']
if os == 'Linux'
  sysinit += sysinit_linux
endif

boot = [
  'bootmisc',
  'fsck',
  'hostname',
  'localmount',
  'loopback',
  'root',
  'swap',
  'sysctl']
boot_bsd = [
  'hostid',
  'newsyslog',
  'savecore',
  'syslogd',
  'urandom']
boot_freebsd = [
  'adjkerntz',
  'dumpon',
  'modules',
  'syscons']
boot_linux = [
  'binfmt',
  'hwclock',
  'keymaps',
  'modules',
  'mtab',
  'procfs',
  'save-keymaps',
  'save-termencoding',
  'seedrng',
  'termencoding']
boot_netbsd = [
  'devdb',
  'swap-blk',
  'tys',
  'wscons']
if get_option('newnet')
  boot += [
    'network',
    'staticroute']
endif
if os == 'DragonFly'
  boot += boot_bsd
elif os == 'FreeBSD' or os == 'GNU-kFreeBSD'
  boot += boot_bsd + boot_freebsd
elif os == 'Linux'
  boot += boot_linux
elif os == 'NetBSD'
  boot += boot_bsd + boot_netbsd
endif

default = [
  'local',
  'netmount']

nonetwork=['local']

shutdown = ['savecache']
shutdown_linux = [
  'killprocs',
  'mount-ro']
if os == 'Linux'
  shutdown += shutdown_linux
endif

runlevels = {
  'sysinit': sysinit,
  'boot': boot,
  'default': default,
  'nonetwork': nonetwork,
  'shutdown': shutdown
  }

foreach runlevel: runlevels.keys()
  foreach service: runlevels[runlevel]
    install_symlink(service,
      install_dir: runlevel_dir / runlevel,
      pointing_to: init_d_dir / service) 
  endforeach
  if get_option('sysvinit') == true and os == 'Linux' and runlevel == 'default'
    foreach tty : get_option('agetty')
      install_symlink('agetty.' + tty,
        install_dir: runlevel_dir / runlevel,
        pointing_to: init_d_dir / 'agetty.' + tty)
    endforeach
  endif
endforeach
