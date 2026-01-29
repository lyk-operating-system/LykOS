cc = meson.get_compiler('c')

custom_target(
    'nvme',
    input: ['main.c', 'nvme.c'],
    output: ['nvme.o'],
    command: [
        cc.cmd_array(),
        c_flags,
        '-c', '@INPUT@',
        '-o', '@OUTPUT@',
    ],
    install: true,
    install_dir: 'bin',
)
