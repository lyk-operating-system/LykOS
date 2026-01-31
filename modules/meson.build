project(
    'modules',
    ['c'],
    version: '0.1',
    meson_version: '>=0.64.0',
)

arch = get_option('arch')
enabled_modules = get_option('enabled_modules')
kernel_include_dir = get_option('kernel_include_dir')

c_flags = [
    # Warnings
    '-Werror',
    '-Wall',
    '-Wextra',
    '-Wno-unused-parameter',
    '-Wno-unused-function',
    '-Wvla',
    '-Wno-implicit-fallthrough',
    # Environment
    '-ffreestanding',
    '-fno-stack-protector',
    '-fno-stack-check',
    '-fno-pic',
    '-fno-pie',
    '-fno-plt',
    # Optimization
    '-O2',
    # Include directory
    '-I', kernel_include_dir,
    '-std=gnu23',
]

if arch == 'x86_64'
    c_flags += [
        '-target', 'x86_64-elf',
        '-m64',
        '-mgeneral-regs-only',
        '-mno-red-zone',
        '-mcmodel=large',
    ]
elif arch == 'aarch64'
    c_flags += [
        '-target', 'aarch64-unknown-none',
        '-mgeneral-regs-only',
    ]
else
    error('Unsupported architecture: ' + arch)
endif

subdir('bus')
subdir('misc')
subdir('storage')
