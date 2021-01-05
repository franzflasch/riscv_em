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

int simple_uart_write(void *priv, rv_uint_xlen address_internal, rv_uint_xlen val, uint8_t nr_bytes)
{
    simple_uart_td *uart = priv;
    uint8_t val_u8 = val;

    pthread_mutex_lock(&uart->lock);

    if(nr_bytes != 1)
        die_msg("UART WRITE: Only single byte access allowed!\n");

    switch(address_internal)
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
            die_msg("UART-Write Reg " PRINTF_FMT " not supported yet!\n", address_internal);
    }

    pthread_mutex_unlock(&uart->lock);

    return RV_MEM_ACCESS_OK;
}

int simple_uart_read(void *priv, rv_uint_xlen address_internal, rv_uint_xlen *outval)
{
    simple_uart_td *uart = priv;
    uint8_t val_u8 = 0;

    pthread_mutex_lock(&uart->lock);

    switch(address_internal)
    {
        case SIMPLE_UART_TX_RX_REG:
            fifo_out(&uart->rx_fifo, &val_u8, 1);
            *outval = val_u8;
        break;
        case SIMPLE_UART_STATUS_REG:
            val_u8 |= fifo_is_empty(&uart->rx_fifo)<<SIMPLE_UART_RXEMPTY_BIT;
            val_u8 |= fifo_is_empty(&uart->tx_fifo)<<SIMPLE_UART_TXEMPTY_BIT;
            val_u8 |= uart->rx_irq_enabled<<SIMPLE_UART_RXIEN_BIT;
            val_u8 |= uart->tx_irq_enabled<<SIMPLE_UART_TXIEN_BIT;
            *outval = val_u8;
        break;
        default:
            die_msg("UART-Read Reg " PRINTF_FMT " not supported yet!\n", address_internal);
    }

    pthread_mutex_unlock(&uart->lock);

    return RV_MEM_ACCESS_OK;
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
