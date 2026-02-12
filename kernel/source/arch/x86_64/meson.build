subdir('abi')
subdir('devices')
subdir('tables')

asm_files += files(
    'syscall.asm',
    'thread.asm',
    'userspace.asm',
)

# Optimised mem functions only perform better on real hardware or KVM accelerated VMs
if build_type == 'release'
    asm_files += files(
        'mem.asm',
    )
endif

c_files += files(
    'cpuid.c',
    'entry.c',
    'fpu.c',
    'int.c',
    'ioport.c',
    'lcpu.c',
    'msr.c',
    'paging.c',
    'serial.c',
    'syscall.c',
    'tcb.c',
    'thread.c',
)
