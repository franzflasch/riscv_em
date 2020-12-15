#ifndef UART_16550A_H
#define UART_16550A_H

#include <stdint.h>

#define UART_16550A_NR_REGS 12

typedef struct uart_16550a_struct
{
    int dlab;
    uint8_t regs[UART_16550A_NR_REGS];

} uart_16550a_td;

int uart_write(void *priv, rv_uint_xlen address_internal, rv_uint_xlen val, uint8_t nr_bytes);
int uart_read(void *priv, rv_uint_xlen address_internal, rv_uint_xlen *outval);

#endif /* UART_16550A_H */
