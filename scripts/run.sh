#!/usr/bin/env bash

ARCH="${1:-x86_64}" # Fallback to x86_64 if no arch is provided.
LYKOS_ISO=$(chariot --option arch=${ARCH} path -r custom/image)/lykos.iso
OVMF="qemu/edk2-ovmf/ovmf-code-${ARCH}.fd"

QEMU_FLAGS=(
    -m 2G
    -smp 4
    -no-shutdown
    -no-reboot
    -cdrom "$LYKOS_ISO"
    -boot d
    -d int
    -D qemu/log.txt
    -serial file:/dev/stdout
    -monitor stdio
    -bios "$OVMF"
)

# Fetch OVMF if missing
if [ ! -f "$OVMF" ]; then
    mkdir -p qemu
    curl -Lo qemu/edk2-ovmf.tar.gz https://github.com/osdev0/edk2-ovmf-nightly/releases/download/nightly-20251210T012647Z/edk2-ovmf.tar.gz
    tar -xzf qemu/edk2-ovmf.tar.gz -C qemu
fi

# Run QEMU
if [ "$ARCH" = "x86_64" ]; then
    qemu-system-x86_64 -M q35 "${QEMU_FLAGS[@]}"
elif [ "$ARCH" = "aarch64" ]; then
    qemu-system-aarch64 -M virt,gic-version=2 -cpu cortex-a72 -device ramfb -device qemu-xhci -device usb-kbd -device usb-mouse "${QEMU_FLAGS[@]}"
fi
