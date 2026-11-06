test_query_exe = executable('test_query',
                            [
                              'test_utils.cpp',
                              'test_query.cpp',
                            ],
                            dependencies: libmado_dep)

test_interpreter_exe = executable('test_interpreter',
                                  [
                                    'test_utils.cpp',
                                    'test_interpreter.cpp',
                                  ],
                                  dependencies: libmado_dep)

test_repository_exe = executable('test_repository',
                                 [
                                   'test_utils.cpp',
                                   'test_repository.cpp',
                                 ],
                                 cpp_args: ['-DMADO_TEST_ROOT="@0@"'.format(meson.project_source_root())],
                                 dependencies: libmado_dep)

test('query', test_query_exe)
test('interpreter', test_interpreter_exe)
test('repository', test_repository_exe)
test('cli', import('python').find_installation(),
     args: [files('test_cli.py'), cli_exe, meson.project_source_root()],
     timeout: 120)
