thirdparty_inc = include_directories('3rdparty')

cli_exe = executable('mado',
                     [
                       # cli
                       'main.cpp',
                       'command.cpp',
                       'entry_info_format.cpp',
                     ],
                     include_directories: thirdparty_inc,
                     dependencies: libmado_dep)
