project('OpenRC', 'c',
  version : '0.44.6',
  license: 'BSD-2',
  default_options : [
    'c_std=c99',
	'prefix=/usr',
    'warning_level=3',
    ],
  meson_version : '>=0.49.0')

cc = meson.get_compiler('c')

audit_dep = dependency('audit', required : get_option('audit'))
if audit_dep.found()
  cc_audit_flags = '-DHAVE_AUDIT'
  else
  cc_audit_flags = []
  endif

if get_option('branding') != ''
  cc_branding_flags = '-DBRANDING=' + get_option('branding')
else
  cc_branding_flags = []
endif

libname = get_option('libdir').split('/')[-1]

option_local_prefix = get_option('local_prefix')
if option_local_prefix == ''
  local_prefix = get_option('prefix') / 'usr' / 'local'
else
  local_prefix = option_local_prefix
endif

option_os = get_option('os')
if option_os == ''
  uname = find_program('uname')
  r = run_command(uname, '-s')
  os = r.stdout().strip()
  os = '-'.join(os.split('/'))
else
  os = option_os
endif

pam = get_option('pam')
if pam
  libpam = cc.find_library('pam')
  libpam_misc = cc.find_library('pam_misc')
  cc_pam_flags = '-DHAVE_PAM'
else
  libpam = []
  libpam_misc = []
  cc_pam_flags = []
endif

option_pkg_prefix = get_option('pkg_prefix')
if option_pkg_prefix == ''
  if os == 'Dragonfly' or os == 'FreeBSD'
    pkg_prefix = '/usr/local'
  elif os == 'GNU' or os == 'GNU-kFreeBSD' or os == 'Linux'
    pkg_prefix = '/usr'
  elif os == 'NetBSD'
    pkg_prefix = '/usr/pkg'
  endif
else
  pkg_prefix = option_pkg_prefix
endif

if get_option('split-usr') == 'auto'
  split_usr = run_command('test', '-L', '/bin').returncode() != 0
else
  split_usr = get_option('split-usr') == 'true'
endif

rootprefix = get_option('rootprefix')
rootprefix_default = '/'
if rootprefix == ''
rootprefix = rootprefix_default
endif

bindir = rootprefix / get_option('bindir')
libdir = rootprefix / get_option('libdir')
libexecdir = get_option('libexecdir')
if os == 'Linux' and libexecdir == 'libexec'
  libexecdir = 'lib'
endif
libexecdir = rootprefix / libexecdir
rc_libexecdir = libexecdir / 'rc'
sbindir = rootprefix / get_option('sbindir')

selinux_dep = dependency('libselinux', required : get_option('selinux'))
if selinux_dep.found()
  cc_selinux_flags = '-DHAVE_SELINUX'
  else
  cc_selinux_flags = []
endif

termcap = get_option('termcap')
if termcap != ''
  termcap_dep = dependency(termcap)
  termcap_flags = '-DHAVE_TERMCAP'
  else
  termcap_dep = []
  termcap_flags = []
endif

if get_option('buildtype').startswith('debug')
  cc_debug_flags = ['-DRC_DEBUG']
else
  cc_debug_flags = []
endif

if os == 'Linux' or os == 'GNU-kFreeBSD'
  cc_os_flags = ['-D_DEFAULT_SOURCE']
elif os == 'FreeBSD'
  cc_os_flags = ['-D_BSD_SOURCE']
elif os == 'GNU'
  cc_os_flags = ['-D_DEFAULT_SOURCE', '-DMAXPATHLEN=4096', '-DPATH_MAX=4096']
  endif

# Try and use some good cc flags if we're building from git. We don't use
# -pedantic as it will warn about our perfectly valid use of %m in our logger.
# We should be using -Wredundant-decls, but our library hidden proto stuff gives
# loads of warnings. I don't fully understand it (the hidden proto, not the
# warning) so we just silence the warning.
cc_warning_flags_test = [
  '-Wcast-align',
  '-Wcast-qual',
  '-Wdeclaration-after-statement',
  '-Wformat=2',
  '-Winline',
  '-Wmissing-declarations',
  '-Wmissing-format-attribute',
  '-Wmissing-noreturn',
  '-Wmissing-prototypes',
  '-Wnested-externs',
  '-Wpointer-arith',
  '-Wsequence-point',
  '-Wshadow',
  '-Wwrite-strings',
  '-Werror=implicit-function-declaration',
  ]
cc_warning_flags = cc.get_supported_arguments(cc_warning_flags_test)
cc_flags = [cc_debug_flags, cc_os_flags, cc_warning_flags]
add_project_arguments(cc_flags, language : 'c')

incdir = include_directories('src/includes')
einfo_incdir = include_directories('src/libeinfo')
rc_incdir = include_directories('src/librc')

init_d_conf_data = configuration_data()
init_d_conf_data.set('SBINDIR', sbindir)
init_d_conf_data.set('PKG_PREFIX', pkg_prefix)
init_d_conf_data.set('SYSCONFDIR', get_option('sysconfdir'))

dl_dep = cc.find_library('dl', required: false)
util_dep = cc.find_library('util', required: false)

if get_option('bash-completions')
  subdir('bash-completion')
endif
subdir('conf.d')
subdir('etc')
subdir('init.d')
subdir('local.d')
subdir('man')
if get_option('pam')
subdir('pam')
  endif
if get_option('pkgconfig')
subdir('pkgconfig')
  endif
subdir('scripts')
subdir('sh')
subdir('src')
subdir('support')
subdir('sysctl.d')
if get_option('zsh-completions')
  subdir('zsh-completion')
endif

meson.add_install_script('tools/meson_runlevels.sh',
  os,
  get_option('newnet') ? 'yes' : 'no',
  rc_libexecdir,
  get_option('sysconfdir'),
  get_option('sysvinit') ? 'yes' : 'no')
meson.add_install_script('tools/meson_final.sh',
  rc_libexecdir,
  os)
