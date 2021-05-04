#ifndef RISCV_CONFIG_H
#define RISCV_CONFIG_H

#define CSR_SUPPORT /* This is currently mandatory, because of the qemu reset-vector */
#define ATOMIC_SUPPORT
#define MULTIPLY_SUPPORT
#define PMP_SUPPORT

#define MROM_BASE_ADDR 0x1000UL
#define MROM_SIZE_BYTES 0xf000UL

#define RAM_BASE_ADDR 0x80000000UL
#define RAM_SIZE_BYTES 0x8000000UL /* 128MB such as the default for the qemu virt machine */

#define CLINT_BASE_ADDR 0x2000000UL
#define CLINT_SIZE_BYTES 0x10000UL

#define SIMPLE_UART_TX_REG_ADDR 0x3000000UL
#define SIMPLE_UART_SIZE_BYTES 0x2

#define PLIC_BASE_ADDR 0x0C000000UL
#define PLIC_SIZE_BYTES 0x3FFF004UL

#define UART8250_TX_REG_ADDR 0x10000000UL

#define RV_EXTENSION_TO_MISA(extension) (1 << (extension - 'A'))
#define RV_SUPPORTED_EXTENSIONS ( RV_EXTENSION_TO_MISA('I') | \
                                  RV_EXTENSION_TO_MISA('M') | \
                                  RV_EXTENSION_TO_MISA('A') | \
                                  RV_EXTENSION_TO_MISA('S') | \
                                  RV_EXTENSION_TO_MISA('U') )

#endif /* RISCV_CONFIG_H */
