project('OpenRC', 'c',
  version : '0.62',
  license: 'BSD-2',
  default_options : [
    'bindir=/bin',
    'c_std=c99',
    'prefix=/usr',
    'sbindir=/sbin',
    'warning_level=3',
    ],
  meson_version : '>=0.62.0'
)

cc = meson.get_compiler('c')
fs = import('fs')

c_flags = []

audit_dep = dependency('audit', required : get_option('audit'))
if audit_dep.found()
  c_flags += '-DHAVE_AUDIT'
endif

if get_option('branding') != ''
  c_flags += '-DBRANDING=' + get_option('branding')
endif

libname = get_option('libdir').split('/')[-1]

local_prefix = get_option('local_prefix')
if local_prefix == ''
  local_prefix = get_option('prefix') / 'usr' / 'local'
endif

os = host_machine.system()
if os != 'linux'
  cap_dep = []
  kvm_dep = cc.find_library('kvm')
else
  cap_dep = dependency('libcap', version: '>=2.33')
  kvm_dep = []
endif

pam_dep = []
if get_option('pam')
  pam_dep = dependency('pam', required: false)
  if not pam_dep.found()
    pam_dep = cc.find_library('pam') # bsds don't ship pam's pkg-config for whatever reason
  endif
  c_flags += '-DHAVE_PAM'
endif

pkg_prefix = get_option('pkg_prefix')
if pkg_prefix == ''
  if os == 'dragonfly' or os == 'freebsd'
    pkg_prefix = '/usr/local'
  elif os == 'gnu' or os == 'linux'
    pkg_prefix = '/usr'
  elif os == 'netbsd'
    pkg_prefix = '/usr/pkg'
  endif
endif

bindir = get_option('prefix') / get_option('bindir')
libdir = get_option('prefix') / get_option('libdir')
pluginsdir = libdir / 'rc/plugins'
libexecdir = get_option('prefix') / get_option('libexecdir')
rc_libexecdir = libexecdir / 'rc'
rc_bindir = rc_libexecdir / 'bin'
rc_sbindir = rc_libexecdir / 'sbin'
rc_shdir = rc_libexecdir / 'sh'
sbindir = get_option('prefix') / get_option('sbindir')
pamdir = get_option('sysconfdir') / 'pam.d'

pam_libdir = get_option('pam_libdir')
if pam_libdir == ''
  pam_libdir = libdir / 'security'
endif

selinux_dep = dependency('libselinux', required: get_option('selinux'))
pam_misc_dep = []
crypt_dep = []
if selinux_dep.found()
  c_flags += '-DHAVE_SELINUX'
  if get_option('pam')
    pam_misc_dep = dependency('pam_misc')
  else
    crypt_dep = dependency('libcrypt')
  endif
endif

if get_option('buildtype').startswith('debug')
  c_flags += '-DRC_DEBUG'
endif

if os == 'linux'
  c_flags += '-D_DEFAULT_SOURCE'
elif os == 'freebsd'
  c_flags += '-D_BSD_SOURCE'
elif os == 'gnu'
  c_flags += ['-D_DEFAULT_SOURCE', '-DPATH_MAX=4096']
endif

# The following warnings are not included in warning_level = 3, but we
# are keeping them for now.
c_flags += cc.get_supported_arguments([
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
  '-Wshadow',
  '-Wwrite-strings',
  '-Werror=implicit-int',
  '-Werror=implicit-function-declaration',
  '-Werror=int-conversion',
  '-Werror=incompatible-function-pointer-types',
])
add_project_arguments(c_flags, language : 'c')

feature_macros = {}

# Meson's has_function_attribute doesn't know about GCC's extended
# version (with arguments), so we have to build a test program instead.
malloc_attribute_test = '''#include<stdlib.h>
__attribute__ ((malloc (free, 1)))
int func() { return 0; }
'''
if cc.compiles(malloc_attribute_test, name : 'malloc attribute with arguments')
  feature_macros += { '-DHAVE_MALLOC_EXTENDED_ATTRIBUTE': 1 }
endif

if cc.has_function('close_range', prefix: '#define _GNU_SOURCE\n#include <unistd.h>')
  feature_macros += { '-DHAVE_CLOSE_RANGE': 1, '-D_GNU_SOURCE': 1 }
elif cc.has_header('linux/close_range.h')
  feature_macros += { '-DHAVE_LINUX_CLOSE_RANGE_H': 1 }
endif

if cc.has_function('strlcpy', prefix: '#include <string.h>')
  feature_macros += { '-DHAVE_STRLCPY': 1 }
elif cc.has_function('strlcpy', prefix: '#define _GNU_SOURCE\n#include <string.h>')
  feature_macros += { '-DHAVE_STRLCPY': 1, '-D_GNU_SOURCE': 1 }
endif

if not cc.has_function('pipe2', prefix: '#include <unistd.h>')
  if cc.has_function('pipe2', prefix: '#define _GNU_SOURCE\n#include <unistd.h>')
    feature_macros += { '-D_GNU_SOURCE': 1 }
  else
    error('C function pipe2() not found')
  endif
endif

add_project_arguments(feature_macros.keys(), language: 'c')

incdir = include_directories('src/shared')
einfo_incdir = include_directories('src/libeinfo')
rc_incdir = include_directories('src/librc')

init_d_conf_data = configuration_data()
init_d_conf_data.set('SBINDIR', sbindir)
init_d_conf_data.set('PKG_PREFIX', pkg_prefix)
init_d_conf_data.set('SYSCONFDIR', get_option('sysconfdir'))
init_d_conf_data.set('RC_LIBEXECDIR', rc_libexecdir)

dl_dep = cc.find_library('dl', required: false)
util_dep = cc.find_library('util', required: false)

subdir('bash-completion')
subdir('conf.d')
subdir('etc')
subdir('init.d')
subdir('local.d')
subdir('man')
subdir('runlevels')
subdir('sh')
subdir('src')
subdir('support')
subdir('sysctl.d')
subdir('test')
subdir('zsh-completion')

if get_option('pkgconfig')
  pkg = import('pkgconfig')

  pkg.generate(
    name : 'einfo',
    description : 'Pretty console informational display',
    version : meson.project_version(),
    libraries : einfo,
  )

  pkg.generate(
    name : 'OpenRC',
    filebase : 'openrc',
    description : 'Universal init system',
    version : meson.project_version(),
    libraries : rc,
    variables: {
      'pluginsdir': pluginsdir,
    }
  )
endif

meson.add_install_script('tools/meson_final.sh',
  rc_libexecdir,
  os)
