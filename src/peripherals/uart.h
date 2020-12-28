#ifndef UART_16550A_H
#define UART_16550A_H

#include <stdint.h>

#include <fifo.h>

#define UART_16550A_NR_REGS 12
#define UART_16550A_FIFO_SIZE 16

typedef struct uart_16550a_struct
{
    uint8_t dlab;

    /* IRQ */
    uint8_t irq_enabled_rx_data_available;
    uint8_t irq_enabled_tx_holding_reg_empty;

    /* FCR */
    uint8_t fifo_enabled;
    uint8_t rx_irq_fifo_level;

    /* fifos */
    fifo_t tx_fifo;
    uint8_t tx_fifo_data[UART_16550A_FIFO_SIZE];
    uint8_t tx_needs_flush;

    fifo_t rx_fifo;
    uint8_t rx_fifo_data[UART_16550A_FIFO_SIZE];
    uint8_t rx_val;

    uint8_t regs[UART_16550A_NR_REGS];

    pthread_mutex_t lock;

} uart_16550a_td;

void uart_init(uart_16550a_td *uart);
int uart_write(void *priv, rv_uint_xlen address_internal, rv_uint_xlen val, uint8_t nr_bytes);
int uart_read(void *priv, rv_uint_xlen address_internal, rv_uint_xlen *outval);
uint8_t uart_update(void *priv);
void uart_add_rx_char(uart_16550a_td *uart, uint8_t x);

#endif /* UART_16550A_H */
