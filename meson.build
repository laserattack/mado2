project('mado2',
        'cpp',
        default_options: [
          'cpp_std=c++17',
          'warning_level=2', # -Wall -Wextra
        ]
       )

inc = include_directories('src')

mado_lib = static_library('mado',
                          [
                            'src/common/fuzzy_match.cpp',
                            'src/common/text_utils.cpp',
                            'src/libmado/query/token.cpp',
                            'src/libmado/query/lexer.cpp',
                            'src/libmado/query/ast.cpp',
                            'src/libmado/query/parser.cpp',
                          ],
                          include_directories: inc,
                         )

test_exe = executable('test_query',
                      [
                        'src/libmado/query/test.cpp',
                        'src/common/test_utils.cpp'
                      ],
                      link_with: mado_lib,
                      include_directories: inc,
                     )
