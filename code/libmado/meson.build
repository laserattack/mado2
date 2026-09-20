libmado_inc = include_directories('include')
libmado_private_inc = include_directories('3rdparty')

libmado_src = [
  # 3rdparty
  '3rdparty/utf8proc/utf8proc.c',

  # utils
  'src/common/fuzzy_match.cpp',
  'src/common/text_utils.cpp',
  'src/common/time_utils.cpp',

  # query
  'src/query/token.cpp',
  'src/query/lexer.cpp',
  'src/query/ast.cpp',
  'src/query/parser.cpp',

  # entry
  'src/entry/entry.cpp',
]

libmado = static_library('mado',
                         libmado_src,
                         include_directories: [libmado_inc, libmado_private_inc])

libmado_dep = declare_dependency(
  link_with: libmado,
  include_directories: libmado_inc)
