libmado_inc = include_directories('include')
libmado_private_inc = include_directories('3rdparty')

libmado_src = [
  # 3rdparty
  # Сторонние зависимости.
  '3rdparty/utf8proc/utf8proc.c',

  # utils
  # Вспомогательные функции.
  'src/common/fuzzy_match.cpp',
  'src/common/glob.c',
  'src/common/text_utils.cpp',
  'src/common/time_utils.cpp',

  # query
  # Запрос.
  'src/query/token.cpp',
  'src/query/lexer.cpp',
  'src/query/time_macro.cpp',
  'src/query/ast.cpp',
  'src/query/parser.cpp',

  # interpreter
  # Интерпретатор.
  'src/interpreter/interpreter.cpp',

  # entry
  # Запись.
  'src/entry/entry.cpp',

  # repository
  # Репозиторий.
  'src/repository/repository.cpp',
]

libmado = static_library('mado',
                         libmado_src,
                         include_directories: [libmado_inc, libmado_private_inc])

libmado_dep = declare_dependency(
  link_with: libmado,
  include_directories: libmado_inc)
