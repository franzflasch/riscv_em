#!/bin/bash

TESTS_DIR="../../../riscv-tests/isa/rv32ui"

riscv32-none-elf-gcc -march=rv32i -I. -I${TESTS_DIR}/../macros/scalar/ -Wl,-T,link.ld,-Bstatic,--strip-debug -ffreestanding -nostdlib -o $1.elf ${TESTS_DIR}/$1.S
riscv32-none-elf-objcopy -O binary $1.elf $1.bin

