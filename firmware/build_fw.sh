riscv32-none-elf-gcc -march=rv32i -Wl,-Bstatic,-T,sections.lds,--strip-debug -ffreestanding -nostdlib -o hx8kdemo_fw.elf start.s main.c

# print text section
riscv32-none-elf-objdump -d hx8kdemo_fw.elf | awk '{print "0x"$2","}'

# dump all
riscv32-none-elf-objdump -d hx8kdemo_fw.elf 

#riscv32-none-elf-gcc -march=rv32i -Wl,-Bstatic,-T,sections.lds,--strip-debug -ffreestanding -nostdlib -o hx8kdemo_fw.elf start.s
