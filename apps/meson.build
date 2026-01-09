project(
    'apps',
    ['c'],
    version: '0.1',
    meson_version: '>=0.64.0',
)

add_project_link_arguments(
    '-static',
    language: 'nasm',
)

add_languages('nasm')

executable(
    'init',
    'init.c',
    install: true,
    install_dir: 'bin',
)
