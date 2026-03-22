project(
    'apps',
    ['c'],
    version: '0.1',
    meson_version: '>=0.64.0',
)

executable(
    'init',
    'init.c',
    link_args: ['-static', '-no-pie', '-O0', '-g'],
    pie: false,
    install: true,
    install_dir: 'bin',
)

executable(
    'client',
    'client.c',
    link_args: ['-static', '-no-pie', '-O0', '-g'],
    pie: false,
    install: true,
    install_dir: 'bin',
)

executable(
    'server',
    'server.c',
    link_args: ['-static', '-no-pie', '-O0', '-g'],
    pie: false,
    install: true,
    install_dir: 'bin',
)
