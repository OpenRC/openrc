# Copyright (c) 2007-2017 The OpenRC Authors.
# See the Authors file at the top-level directory of this distribution and
# https://github.com/OpenRC/openrc/blob/master/AUTHORS
#
# This file is part of OpenRC. It is subject to the license terms in
# the LICENSE file found in the top-level directory of this
# distribution and at https://github.com/OpenRC/openrc/blob/master/LICENSE
# This file may not be copied, modified, propagated, or distributed
# except according to the terms contained in the LICENSE file.

project('openrc', 'c',
        license: 'Copyright (c) 2007-2017, the OpenRC authors',
        version: '0.33.1',
        meson_version: '>= 0.40',
        default_options: [
          'warning_level=1',
          'prefix=/usr',
          'libdir=lib',
          'c_std=c99', # Default to using the C99 standard
        ],
       )

mk_prefix = get_option('mk-prefix')
rc_prefix = get_option('rc-prefix')
rc_libname = get_option('rc-libname')
rc_sysconfdir = get_option('rc-sysconfdir')
rc_libexecdir = get_option('rc-libexecdir')
rc_pkg_prefix = get_option('rc-pkg-prefix')
rc_local_prefix = get_option('rc-local-prefix')
rc_var_base = get_option('rc-var-prefix')

mk_pkgconfig = get_option('pkgconfig')
mk_net = get_option('net')
mk_pam = get_option('pam')
mk_selinux = get_option('selinux')
mk_sysvinit = get_option('sysvinit')
mk_termcap = get_option('termcap')
mk_audit = get_option('audit')

if not meson.is_cross_build()
  # meson's system() function will return the operating system name ['linux',
  # 'windows', 'darwin', 'cygwin', 'bsd'], but return bsd for all *BSD OSes, so
  # using uname to detect os

  # auto check os when do native build.
  os = run_command('uname',  '-s').stdout().strip()
  os = '-'.join(os.split('/'))
else
  # using os option's value when do cross build.
  os = get_option('os')
endif

if rc_prefix == ''
  uprefix = '/usr'
  prefix = mk_prefix ? '/usr' : '/'
else
  uprefix = mk_prefix ? join_paths(re_prefix, 'usr') : re_prefix
  prefix = mk_prefix ? join_paths(re_prefix, 'usr') : rc_prefix
endif

local_prefix = rc_local_prefix != '' ? rc_local_prefix : join_paths(uprefix, 'local')
sysconfdir = rc_prefix != '' ? join_paths(re_prefix, 'etc') : '/etc'
initdir = join_paths(sysconfdir, 'init.d')
confdir = join_paths(sysconfdir, 'conf.d')
localdir = join_paths(sysconfdir, 'local.d')
sysctldir = join_paths(sysconfdir, 'sysctl.d')

bindir = join_paths(prefix, 'bin')
binmode = 'rwxr-xr-x' # 0755
sbindir = join_paths(prefix, 'sbin')
sbinmode = 'rwxr-xr-x' # 0755
incdir = join_paths(prefix, 'include')
incmode = 'r--r--r--' # '0444'

if rc_libname != ''
  libname = rc_libname
else
  libname = run_command('readlink', '/lib').stdout().strip()
  if libname.contains('lib64')
    libname = 'lib64'
  else
    libname = 'lib'
  endif
endif

confdir = join_paths(rc_sysconfdir, 'conf.d')
libdir = join_paths(uprefix, libname)
libmode = 'r--r--r--' # '0444'
shlibdir = join_paths(prefix, libname)
libexecdir = rc_libexecdir != '' ? rc_libexecdir : join_paths(prefix, 'libexec', 'rc')
manprefix = join_paths(uprefix, 'share')

mandir = join_paths(manprefix, 'man')
manmode = 'r--r--r--' # '0444'

datadir = join_paths(uprefix, 'share', 'openrc')
datamode = 'rw-r--r--' # '0644'

docdir = join_paths(uprefix, 'share', 'doc')
docmode = 'rw-r--r--' # '0644'
confmode = 'rw-r--r--' # '0644'

pkgconfiglibdir = join_paths(libdir, 'pkgconfig')


cc = meson.get_compiler('c')

# Try and use some good cc flags if we're building from git. We don't use
# -pedantic as it will warn about our perfectly valid use of %m in our logger.
# We should be using -Wredundant-decls, but our library hidden proto stuff gives
# loads of warnings. I don't fully understand it (the hidden proto, not the
# warning) so we just silence the warning.
foreach arg : [
  '-Wall',
  '-Wextra',
  '-Wimplicit',
  '-Wshadow',
  '-Wformat=2',
  '-Wmissing-prototypes',
  '-Wmissing-declarations',
  '-Wmissing-noreturn',
  '-Wmissing-format-attribute',
  '-Wnested-externs',
  '-Winline',
  '-Wwrite-strings',
  '-Wcast-align',
  '-Wcast-qual',
  '-Wpointer-arith',
  '-Wdeclaration-after-statement',
  '-Wsequence-point',
  '-Werror=implicit-function-declaration',
]
  if cc.has_argument(arg)
    add_project_arguments(arg, language : 'c')
  endif
endforeach

library_deps = []
common_compile_args = []

mk_pam = get_option('pam')
if mk_pam != 'false'
  libpam = cc.find_library('pam', required : mk_pam == 'true')
  libpam_misc = cc.find_library('pam_misc', required : mk_pam == 'true')
  if libpam.found() and libpam_misc.found()
    common_compile_args += ['-DHAVE_PAM']
  endif
endif

mkselinux = false
if mk_selinux != 'false'
  libselinux = dependency('libselinux',
                          version : '>= 2.1.9',
                          required : mk_selinux == 'true')
  if libselinux.found()
    common_compile_args += ['-DHAVE_SELINUX']
    mkselinux = true
  endif
endif

mkaudit = false
if mk_audit != 'false'
  libaudit = dependency('audit', required : mk_audit == 'true')
  if libaudit.found()
    common_compile_args += ['-DHAVE_AUDIT']
    mkaudit = true
  endif
endif

mktermcap = false
if mk_termcap == 'ncurses'
  libncurses = dependency('ncurses', required : false)
  if not libncurses.found()
    libncurses = cc.find_library('ncurses', required : true)
  endif
  common_compile_args += ['-DHAVE_TERMCAP']
  mktermcap = true
elif mk_termcap == 'termcap'
  libtermcap = cc.find_library('termcap', required : true)
  common_compile_args += ['-DHAVE_TERMCAP']
  mktermcap = true
endif


openrc_includes = include_directories('src/includes', 'src/libeinfo', 'src/librc')

libutil = cc.find_library('util')
library_deps += [libutil]

substs = configuration_data()
if os == 'Linux'
  sfx = '.Linux.in'
  pkg_prefix = rc_pkg_prefix != '' ? rc_pkg_prefix : '/usr'
  common_compile_args += ['-D_DEFAULT_SOURCE', '-D_XOPEN_SOURCE=700']
  add_project_link_arguments('-Wl,-Bdynamic', language : 'c')
  libdl = cc.find_library('dl')
  library_deps += [libdl]
  substs.set('TERM', 'cons25')
elif os == 'FreeBSD'
  sfx = '.BSD.in'
  pkg_prefix = rc_pkg_prefix != '' ? rc_pkg_prefix : '/usr/local'
  common_compile_args += []
  libkvm = cc.find_library('kvm')
  library_deps += [libkvm]
  substs.set('TERM', 'wsvt25')
elif os == 'NetBSD'
  sfx = '.BSD.in'
  pkg_prefix = rc_pkg_prefix != '' ? rc_pkg_prefix : '/usr/pkg'
  common_compile_args += []
  libkvm = cc.find_library('kvm')
  library_deps += [libkvm]
  substs.set('TERM', 'wsvt25')
elif os == 'DragonFly'
  sfx = '.BSD.in'
  pkg_prefix = rc_pkg_prefix != '' ? rc_pkg_prefix : '/usr/local'
  common_compile_args += []
  libkvm = cc.find_library('kvm')
  library_deps += [libkvm]
elif os == 'GNU-kFreeBSD'
  sfx = '.GNU-kFreeBSD.in'
  pkg_prefix = rc_pkg_prefix != '' ? rc_pkg_prefix : '/usr'
  common_compile_args += ['-D_BSD_SOURCE', '-D_XOPEN_SOURCE=700']
  add_project_link_arguments('-Wl,-Bdynamic', language : 'c')
  libdl = cc.find_library('dl')
  library_deps += [libdl]
elif os == 'GNU'
  sfx = '.GNU.in'
  pkg_prefix = rc_pkg_prefix != '' ? rc_pkg_prefix : '/usr'
  common_compile_args += ['-D_DEFAULT_SOURCE',
                          '-D_XOPEN_SOURCE=700',
                          '-DMAXPATHLEN=4096',
                          '-DPATH_MAX=4096',
                         ]
  add_project_link_arguments('-Wl,-Bdynamic', language : 'c')
  libdl = cc.find_library('dl')
  library_deps += [libdl]
else
  error('Unsupported os: '+ os)
endif

substs.set('PREFIX', prefix)
substs.set('LIB', libname)
substs.set('SYSCONFDIR', sysconfdir)
substs.set('LIBEXECDIR', libexecdir)
substs.set('SBINDIR', sbindir)
substs.set('BINDIR', bindir)
substs.set('SYSCONFDIR', sysconfdir)
substs.set('PKG_PREFIX', pkg_prefix)
substs.set('LOCAL_PREFIX', local_prefix)
substs.set('VARBASE', rc_var_base)
substs.set('VERSION', meson.project_version())
substs.set('SHELL', '/bin/sh')

sed = find_program('sed')
# grep = find_program('grep')
# awk = find_program('awk')
# stat = find_program('stat')
git = find_program('git', required : false)

if git.found()
  git_head = run_command(
    git,
    ['--git-dir=@0@/.git'.format(meson.current_source_dir()),
     'rev-parse', 'HEAD']).stdout().strip()
  git_head_short = run_command(
    git,
    ['--git-dir=@0@/.git'.format(meson.current_source_dir()),
     'rev-parse', '--short=7', 'HEAD']).stdout().strip()

  run_target(
    'git-snapshot',
    command : ['git', 'archive',
               '-o', '@0@/openrc-@1@.tar.gz'.format(meson.current_source_dir(),
                                                    git_head_short),
               '--prefix', 'openrc-@0@/'.format(git_head),
               'HEAD'])
endif

subdirs = ['conf.d',
           'etc',
           'init.d',
           'local.d',
           'man',
           'scripts',
           'sh',
           'src',
           'support',
           'sysctl.d',
          ]
foreach subdir : subdirs
  subdir(subdir)
endforeach

if mk_pkgconfig
  subdir('pkgconfig')
endif
