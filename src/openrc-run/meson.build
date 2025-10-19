openrc_run = executable('openrc-run', 'openrc-run.c',
  dependencies: [rc, einfo, shared, audit_dep, dl_dep, pam_dep, pam_misc_dep, selinux_dep, util_dep, crypt_dep],
  include_directories: incdir,
  install: true,
  install_dir: sbindir)
