test_log_entry = executable(
  'test-log-entry',
  'test-log-entry.c',
  include_directories: [
    osnx_common_inc,
    include_directories('..'),
  ],
  dependencies: osnx_common_deps,
  link_with: osnx_common_lib,
)

test('log-entry', test_log_entry)
