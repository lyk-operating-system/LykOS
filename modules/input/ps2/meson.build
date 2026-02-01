cc = meson.get_compiler('c')

custom_target(
    'test_module',
    input: ['main.c', 'ps2.c'],
    output: ['test_module.o'],
    command: [
        cc.cmd_array(),
        c_flags,
        '-r', '@INPUT@',
        '-o', '@OUTPUT@',
    ],
    install: true,
    install_dir: 'bin',
)
