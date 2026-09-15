libmado_inc = include_directories('include')

libmado_src = [
  'src/common/fuzzy_match.cpp',
  'src/common/text_utils.cpp',
  'src/common/time_utils.cpp',
  'src/query/token.cpp',
  'src/query/lexer.cpp',
  'src/query/ast.cpp',
  'src/query/parser.cpp',
]

libmado = static_library('mado',
                         libmado_src,
                         include_directories: libmado_inc)

libmado_dep = declare_dependency(
  link_with: libmado,
  include_directories: libmado_inc,
)
