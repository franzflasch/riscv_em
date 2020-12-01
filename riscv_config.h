#ifndef RISCV_CONFIG_H
#define RISCV_CONFIG_H

#define DEBUG

// #define RV64
#define CSR_SUPPORT

#define RAM_BASE_ADDR 0x80000000

#define RAM_SIZE_BYTES 0x10000
#define NR_RAM_WORDS (0x10000/sizeof(uint32_t))

#endif /* RISCV_CONFIG_H */
