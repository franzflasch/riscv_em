#ifndef UART_NS8250_H
#define UART_NS8250_H

#include <stdint.h>

#include <fifo.h>

#define UART_NS8250_NR_REGS 12
#define UART_NS8250_FIFO_SIZE 16

#define IRQ_PENDING 0
#define NO_IRQ_PENDING 1

typedef struct uart_ns8250_struct
{
    uint8_t dlab;

    /* IRQ */
    uint8_t irq_enabled_rx_data_available;
    uint8_t irq_enabled_tx_holding_reg_empty;
    uint8_t irq_enabled_rlsr_change;
    uint8_t irq_enabled_msr_change;
    uint8_t irq_enabled_sleep;
    uint8_t irq_enabled_low_power;

    uint8_t tx_holding_reg_empty;
    uint8_t tx_holding_irq_cleared;

    uint8_t fifo_enabled;

    fifo_t tx_fifo;
    uint8_t tx_fifo_data[UART_NS8250_FIFO_SIZE];
    uint8_t tx_needs_flush;
    uint8_t tx_stop_triggering;

    fifo_t rx_fifo;
    uint8_t rx_fifo_data[UART_NS8250_FIFO_SIZE];
    uint8_t rx_irq_fifo_level;

    uint8_t rx_val;
    uint8_t rx_data_avail;

    uint8_t regs[UART_NS8250_NR_REGS];

    pthread_mutex_t lock;

} uart_ns8250_td;

void uart_init(uart_ns8250_td *uart);
int uart_write(void *priv, rv_uint_xlen address_internal, rv_uint_xlen val, uint8_t nr_bytes);
int uart_read(void *priv, rv_uint_xlen address_internal, rv_uint_xlen *outval);
uint8_t uart_update(void *priv);
void uart_add_rx_char(uart_ns8250_td *uart, uint8_t x);

#endif /* UART_NS8250_H */
