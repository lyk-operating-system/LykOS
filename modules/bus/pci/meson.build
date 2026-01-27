cc = meson.get_compiler('c')

custom_target(
    'pci',
    input: ['main.c'],
    output: ['pci.o'],
    command: [
        cc.cmd_array(),
        c_flags,
        '-c', '@INPUT@',
        '-o', '@OUTPUT@',
    ],
    install: true,
    install_dir: 'bin',
)
