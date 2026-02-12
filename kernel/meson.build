project(
    'kernel',
    ['c'],
    version: '0.1',
    meson_version: '>=0.64.0',
    default_options: ['c_std=gnu23', 'warning_level=2'],
)

arch = get_option('arch')
build_type = get_option('build_type')

if arch == 'x86_64'
    add_languages('nasm')
endif

nasm = find_program('nasm', required: true)

c_flags = [
    # Warnings
    '-Werror',
    '-Wall',
    '-Wextra',
    '-Wno-unused-parameter',
    '-Wno-unused-function',
    '-Wvla',
    '-Wimplicit-fallthrough',
    # Environment
    '-ffreestanding',
    '-fno-stack-protector',
    '-fno-stack-check',
    '-fno-pic',
    '-fno-pie',
    # Optimization
    '-flto',
    '-ffat-lto-objects',
    '-O2',
    '-g',
    # Features
    '-DPRINTF_DISABLE_SUPPORT_FLOAT',
    '-DLIMINE_API_REVISION=4',
]

as_flags = [
    '-g',
]

nasm_flags = [
    '-f', 'elf64',
    '-g',
]

ld_flags = [
    '-nostdlib',
    '-static',
    '-gc-sections',
    '-z', 'max-page-size=0x1000',
    '-g',
    '-T', meson.current_source_dir() / f'linker-scripts/@arch@.ld',
]

if arch == 'x86_64'
    c_flags += [
        '-target', 'x86_64-elf',
        '-m64',
        '-mgeneral-regs-only',
        '-mno-red-zone',
        '-mcmodel=kernel',
    ]
    ld_flags += [
        '-m', 'elf_x86_64',
    ]
elif arch == 'aarch64'
    c_flags += [
        '-target', 'aarch64-unknown-none',
        '-mgeneral-regs-only',
    ]
    ld_flags += [
        '-m', 'aarch64elf',
    ]
else
    error('Unsupported architecture: ' + arch)
endif

c_files = []
as_files = []
asm_files = []

limine_header = custom_target(
    'limine.h',
    output: 'limine.h',
    command: [
        'curl',
        '-L',
        '-o', 'limine.h',
        'https://codeberg.org/Limine/limine-protocol/raw/branch/trunk/include/limine.h',
    ],
)

include_directories = include_directories('include')

subdir('source')

sources = c_files + as_files + asm_files + [limine_header]

output_static_lib = static_library(
    'kernel',
    sources: sources,
    include_directories: include_directories,
    c_args: c_flags,
    install: false,
)

custom_target(
    input: output_static_lib,
    output: f'kernel-@arch@.elf',
    command: [
        meson.get_external_property('ld'),
        ld_flags,
        '--whole-archive', '@INPUT@',
        '-o', '@OUTPUT@',
    ],
    install: true,
    install_dir: 'bin',
    install_mode: 'rw-r--r--',
)
