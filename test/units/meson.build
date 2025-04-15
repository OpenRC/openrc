test('is_older_than', find_program('check-is-older-than.sh'), env: test_env)
test('sh_yesno', find_program('check-sh-yesno.sh'), env: test_env)
test('rc-update', find_program('rc-update.sh'), args: [find_program('setup-root.sh'), rc_update], env: test_env)
test('rc-service', find_program('rc-service.sh'), env: test_env,
  args: [
    find_program('setup-root.sh'),
    rc_service,
    openrc_run.full_path(),
    openrc_run_sh.full_path(),
    gendepends_sh.full_path()
  ])
