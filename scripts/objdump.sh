#!/usr/bin/env bash

ADDRESS=0x$1

objdump $(chariot path -r package/apps)/usr/bin/init \
    -d -wrC \
    --visualize-jumps=color \
    --disassembler-color=on \
    --start-address=$ADDRESS \
    --stop-address=0x$(printf '%x' $(($ADDRESS + 128)))
