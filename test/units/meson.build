test('is_older_than', find_program('check-is-older-than.sh'), env: test_env)
test('sh_yesno', find_program('check-sh-yesno.sh'), env: test_env)
test('rc-service', find_program('rc-service.sh'),
  env: [test_env, 'SUBDIR=' + meson.current_source_dir()],
  workdir: meson.project_source_root(),
  args: [ rc_service, openrc_run ],
  depends: script_deps)
