#/bin/bash

set -e

riscv32-none-elf-gcc -march=rv32i -Wl,-Bstatic,-T,sections.lds,--strip-debug -ffreestanding -nostdlib -o hello_world_fw.elf start.s main.c
riscv32-none-elf-objcopy -O binary hello_world_fw.elf hello_world_fw.bin

# print text section
#riscv32-none-elf-objdump -d hx8kdemo_fw.elf | awk '{print "0x"$2","}'

# dump all
#riscv32-none-elf-objdump -d hx8kdemo_fw.elf 

