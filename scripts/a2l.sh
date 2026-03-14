#!/usr/bin/env bash

ARCH="${1:-x86_64}"
ADDRESS=0x$2

addr2line -fai -e $(chariot --option arch=${ARCH} path -r custom/kernel)/usr/bin/kernel-${ARCH}.elf $ADDRESS
