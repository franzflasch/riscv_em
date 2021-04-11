#include <stdio.h>
#include <string.h>

#include <riscv_helper.h>
#include <simple_uart.h>

#include <fifo.h>

#define SIMPLE_UART_TX_RX_REG 0
#define SIMPLE_UART_STATUS_REG 1

#define SIMPLE_UART_RXEMPTY_BIT 0
#define SIMPLE_UART_RXIEN_BIT 1
#define SIMPLE_UART_TXEMPTY_BIT 2
#define SIMPLE_UART_TXIEN_BIT 3

void simple_uart_init(simple_uart_td *uart)
{
    memset(uart, 0, sizeof(simple_uart_td));

    if (pthread_mutex_init(&uart->lock, NULL) != 0)
    {
        die_msg("uart mutex init failed\n");
    }

    fifo_init(&uart->rx_fifo, uart->rx_fifo_data, SIMPLE_UART_FIFO_SIZE);
    fifo_init(&uart->tx_fifo, uart->tx_fifo_data, SIMPLE_UART_FIFO_SIZE);
}

rv_ret simple_uart_bus_access(void *priv, privilege_level priv_level, bus_access_type access_type, rv_uint_xlen address, void *value, uint8_t len)
{
    (void) priv_level;
    simple_uart_td *uart = priv;
    uint8_t val_u8 = 0;

    if(len != 1)
        die_msg("UART WRITE: Only single byte access allowed!\n");

    pthread_mutex_lock(&uart->lock);

    if(access_type == bus_write_access)
    {
        val_u8 = *(uint8_t*)value;
        switch(address)
        {
            case SIMPLE_UART_TX_RX_REG:
                fifo_in(&uart->tx_fifo, &val_u8, 1);
                if(val_u8 == '\n')
                    uart->tx_needs_flush = 1;
                // putchar(val_u8);
                // fflush(stdout);
                uart->tx_triggered = 0;
            break;
            case SIMPLE_UART_STATUS_REG:
                uart->rx_irq_enabled = extract8(val_u8, SIMPLE_UART_RXIEN_BIT, 1);
                uart->tx_irq_enabled = extract8(val_u8, SIMPLE_UART_TXIEN_BIT, 1);
                // printf("\n\nrx irq_enabled %x tx irq_enabled %x\n\n", uart->rx_irq_enabled, uart->tx_irq_enabled);
            break;
            default:
                die_msg("UART-Write Reg " PRINTF_FMT " not supported yet!\n", address);
        }
    }
    else
    {
        switch(address)
        {
            case SIMPLE_UART_TX_RX_REG:
                fifo_out(&uart->rx_fifo, &val_u8, 1);
                memcpy(value, &val_u8, 1);
            break;
            case SIMPLE_UART_STATUS_REG:
                val_u8 |= fifo_is_empty(&uart->rx_fifo)<<SIMPLE_UART_RXEMPTY_BIT;
                val_u8 |= fifo_is_empty(&uart->tx_fifo)<<SIMPLE_UART_TXEMPTY_BIT;
                val_u8 |= uart->rx_irq_enabled<<SIMPLE_UART_RXIEN_BIT;
                val_u8 |= uart->tx_irq_enabled<<SIMPLE_UART_TXIEN_BIT;
                memcpy(value, &val_u8, 1);
            break;
            default:
                die_msg("UART-Read Reg " PRINTF_FMT " not supported yet!\n", address);
        }
    }

    pthread_mutex_unlock(&uart->lock);

    return rv_ok;
}


uint8_t simple_uart_update(void *priv)
{
    simple_uart_td *uart = priv;
    uint8_t irq_trigger = 0;
    uint8_t tmp_fifo_len = 0;
    uint8_t tmp_char = 0;
    int i = 0;
    static int count = 0;

    pthread_mutex_lock(&uart->lock);

    if(fifo_is_full(&uart->tx_fifo) || uart->tx_needs_flush)
    {
        tmp_fifo_len = fifo_len(&uart->tx_fifo);
        for(i=0;i<tmp_fifo_len;i++)
        {
            fifo_out(&uart->tx_fifo, &tmp_char, 1);
            putchar(tmp_char);
        }
        fflush( stdout );
        uart->tx_needs_flush = 0;
    }

    if( uart->rx_irq_enabled && (fifo_is_full(&uart->rx_fifo)) ) //&& !uart->rx_triggered )
    {
        //printf("RX IRQ trigger! %d\n", count);
        irq_trigger = 1;
        uart->rx_triggered = 1;
        count++;
    }
    else if( uart->tx_irq_enabled && (fifo_is_empty(&uart->tx_fifo)) )
    {
        // printf("TX IRQ trigger! %d\n", count);
        irq_trigger = 1;
        uart->tx_triggered = 1;
        count++;   
    }

    pthread_mutex_unlock(&uart->lock);

    return irq_trigger;
}

void simple_uart_add_rx_char(simple_uart_td *uart, uint8_t x)
{
    pthread_mutex_lock(&uart->lock);

    fifo_in(&uart->rx_fifo, &x, 1);

    // uart->rx_triggered = 0;
    // printf("rx irq_enabled %x tx irq_enabled %x triggered %x\n", uart->rx_irq_enabled, uart->tx_irq_enabled, uart->tx_triggered);

    pthread_mutex_unlock(&uart->lock);
}
