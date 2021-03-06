#ifndef SIMPLE_UART_H
#define SIMPLE_UART_H

#include <stdint.h>
#include <pthread.h>

#include <fifo.h>

#include <riscv_types.h>

#define SIMPLE_UART_FIFO_SIZE 1

typedef struct simple_uart_struct
{
    uint8_t rx_triggered;
    fifo_t rx_fifo;
    uint8_t rx_fifo_data[SIMPLE_UART_FIFO_SIZE];
    uint8_t rx_irq_enabled;

    uint8_t tx_triggered;
    fifo_t tx_fifo;
    uint8_t tx_fifo_data[SIMPLE_UART_FIFO_SIZE];
    uint8_t tx_irq_enabled;
    uint8_t tx_needs_flush;

    pthread_mutex_t lock;

} simple_uart_td;

void simple_uart_init(simple_uart_td *uart);
rv_ret simple_uart_bus_access(void *priv, privilege_level priv_level, bus_access_type access_type, rv_uint_xlen address, void *value, uint8_t len);
uint8_t simple_uart_update(void *priv);
void simple_uart_add_rx_char(simple_uart_td *uart, uint8_t x);

#endif /* UART_NS8250_H */
