#ifndef RISCV_CONFIG_H
#define RISCV_CONFIG_H

#define DEBUG

#define RV64
#define CSR_SUPPORT
#define ATOMIC_SUPPORT
#define MULTIPLY_SUPPORT

#define MROM_BASE_ADDR 0x1000
#define MROM_SIZE_BYTES 0xf000

#define RAM_BASE_ADDR 0x80000000
#define RAM_SIZE_BYTES 0x8000000 /* 128MB such as the default for the qemu virt machine */

#define CLINT_BASE_ADDR 0x2000000
#define CLINT_SIZE_BYTES 0x10000

#define UART_TX_REG_ADDR 0x10000000

#endif /* RISCV_CONFIG_H */
