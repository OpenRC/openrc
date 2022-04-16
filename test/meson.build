if meson.version().version_compare('>=0.56.0')
  build_root = meson.project_build_root()
  source_root = meson.project_source_root()
else
  build_root = meson.build_root()
  source_root = meson.source_root()
endif

test_env = [
  'BUILD_ROOT=' + build_root,
  'SOURCE_ROOT=' + source_root
  ]

check_obsolete_functions = find_program('check-obsolete-functions.sh')
check_spacing_style = find_program('check-spacing-style.sh')
check_trailing_newlines = find_program('check-trailing-newlines.sh')
check_trailing_whitespace = find_program('check-trailing-whitespace.sh')
check_xfunc_usage = find_program('check-xfunc-usage.sh')

test('check for obsolete functions', check_obsolete_functions, env : test_env)
test('check spacing style', check_spacing_style, env : test_env)
test('check trailing newlines', check_trailing_newlines, env : test_env)
test('check trailing whitespace', check_trailing_whitespace, env : test_env)
test('check xfunc usage', check_xfunc_usage, env : test_env)

subdir('units')
