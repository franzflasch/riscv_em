#include <stdio.h>
#include <string.h>

#include <pthread.h>

#include <riscv_helper.h>
#include <uart.h>

// #define UART_DEBUG
#ifdef UART_DEBUG
#define UART_DBG(...) do{ printf( __VA_ARGS__ ); } while( 0 )
#else
#define UART_DBG(...) do{ } while ( 0 )
#endif

#define REG_RX_TX_DIV_LATCH_LO 0
#define REG_IER_LATCH_HI 1

/*** COMMON REGS ***/
/* READ */
#define REG_IIR 2
/* WRITE */
#define REG_FCR 2
/* READ/WRITE */
#define REG_LCR 3
/* READ/WRITE */
#define REG_MCR 4
/* READ */
#define REG_LSR 5
/* READ */
#define REG_MSR 6
/* READ/WRITE */
#define REG_SCRATCH 7

#define UART_FIFO_FAIL     0
#define UART_FIFO_SUCCESS  1

void uart_init(uart_16550a_td *uart)
{
    memset(uart, 0, sizeof(uart_16550a_td));

    if (pthread_mutex_init(&uart->lock, NULL) != 0)
    {
        die_msg("uart mutex init failed\n");
    }

    fifo_init(&uart->tx_fifo, uart->tx_fifo_data, UART_16550A_FIFO_SIZE);
    fifo_init(&uart->rx_fifo, uart->rx_fifo_data, UART_16550A_FIFO_SIZE);
}

int uart_write(void *priv, rv_uint_xlen address_internal, rv_uint_xlen val, uint8_t nr_bytes)
{
    uart_16550a_td *uart = priv;
    uint8_t tmp_bits = 0;
    uint8_t tmp_val = 0;

    if(nr_bytes != 1)
        die_msg("UART WRITE: Only single byte access allowed!\n");

    switch(address_internal)
    {
        case REG_RX_TX_DIV_LATCH_LO:
            if(!uart->dlab)
            {
                if(uart->fifo_enabled)
                {
                    tmp_val = val;
                    fifo_in(&uart->tx_fifo, &tmp_val, 1);

                    if((tmp_val == '\n') || (tmp_val == '\r'))
                        uart->tx_needs_flush = 1;
                }
                else
                {
                    putchar((char) val);
                }
            }
            else
            {
                /* we do not support any dlab registers */
                UART_DBG("DLAB Write Access " PRINTF_FMT " " PRINTF_FMT "!\n", address_internal, val);
            }
        break;
        case REG_IER_LATCH_HI:
            if(!uart->dlab)
            {
                uart->irq_enabled_rx_data_available = extract8(val, 0, 1);
                uart->irq_enabled_tx_holding_reg_empty = extract8(val, 1, 1);

                if(uart->irq_enabled_tx_holding_reg_empty)
                    die_msg("UART Interrupts currently not supported!\n");

                /* Currently we do not support any other interrupts */
                tmp_val = extract32(val, 2, 6);
                if(tmp_val)
                    die_msg("Currently only RX and TX interrupts supported!\n");
            }
            else
            {
                /* we do not support any dlab registers */
                UART_DBG("DLAB Write Access " PRINTF_FMT " " PRINTF_FMT "!\n", address_internal, val);
            }
        break;
        case REG_FCR:
            /* DMA mode */
            if(CHECK_BIT(val, 3))
                die_msg("DMA mode not supported!\n");

            /* Clear RX fifo */
            if(CHECK_BIT(val, 1))
                fifo_reset(&uart->rx_fifo);

            /* Clear TX fifo */
            if(CHECK_BIT(val, 2))
                fifo_reset(&uart->tx_fifo);

            uart->fifo_enabled = extract8(val, 0, 1);

            tmp_bits = extract8(val, 6, 2);
            uart->rx_irq_fifo_level = (tmp_bits == 3) ? 14 : (tmp_bits == 2) ? 8 : (tmp_bits == 1) ? 4 : 1;
        break;
        case REG_LCR:
            uart->dlab = extract8(val, 7, 1);

            UART_DBG("LCR: "PRINTF_FMT"\n", val);

            if(uart->dlab)
                UART_DBG("dlab activated\n");
            else
                UART_DBG("dlab deactivated\n");
        break;
        case REG_MCR:
            /* nothing to do here */
        break;
        default:
            die_msg("UART-Write Reg " PRINTF_FMT " not supported yet!\n", address_internal);
    }

    return RV_MEM_ACCESS_OK;
}

int uart_read(void *priv, rv_uint_xlen address_internal, rv_uint_xlen *outval)
{
    uart_16550a_td *uart = priv;
    uint8_t tmp_out_val = 0;

    pthread_mutex_lock(&uart->lock);

    switch(address_internal)
    {
        case REG_RX_TX_DIV_LATCH_LO:
            if(!uart->dlab)
            {
                if(uart->fifo_enabled)
                {
                    fifo_out(&uart->rx_fifo, &tmp_out_val, 1);
                    *outval = tmp_out_val;

                    if(fifo_is_empty(&uart->rx_fifo))
                        assign_u8_bit(&uart->regs[REG_LSR], 0, 0);
                }
                else
                {
                    *outval = uart->rx_val;
                    assign_u8_bit(&uart->regs[REG_LSR], 0, 0);
                }
            }
            else
            {
                /* we do not support any dlab registers */
                UART_DBG("DLAB Read Access " PRINTF_FMT "!\n", address_internal);
            }
        break;
        case REG_IER_LATCH_HI:
            if(!uart->dlab)
            {
                tmp_out_val = (uart->irq_enabled_tx_holding_reg_empty << 1) | (uart->irq_enabled_rx_data_available << 0);
                *outval = tmp_out_val;
            }
            else
            {
                /* we do not support any dlab registers */
                UART_DBG("DLAB Read Access " PRINTF_FMT "!\n", address_internal);
                *outval = 0;
            }
        break;
        // case REG_IIR:
        //     /* 1 means no interrupt pending */
        //     *outval = 1;
        // break;
        case REG_LSR:
            /* THR empty and line idle is always true here in our emulation */
            tmp_out_val = (1<<6) | (1<<5);
            *outval = tmp_out_val;
        break;
        case REG_LCR:
            *outval = (uart->dlab<<7) | uart->regs[REG_LCR];
        break;
        // case REG_MSR:
        //     *outval = 0;
        // break;
        default:
            die_msg("UART-Read Reg " PRINTF_FMT " not supported yet!\n", address_internal);
    }

    // /* just for testing */
    // *outval = 0x60;
    // die_msg("UART READ " PRINTF_FMT " currently not supported!\n", address_internal);

    pthread_mutex_unlock(&uart->lock);

    return RV_MEM_ACCESS_OK;
}

uint8_t uart_update(void *priv)
{
    uart_16550a_td *uart = priv;
    int i = 0;
    uint8_t tmp_char = 0;
    uint8_t tmp_fifo_len = 0;
    uint8_t irq_trigger = 0;

    /* check if we need to flush the tx fifo */
    if(uart->fifo_enabled)
    {
        if(fifo_is_full(&uart->tx_fifo) || uart->tx_needs_flush)
        {
            tmp_fifo_len = fifo_len(&uart->tx_fifo);
            for(i=0;i<tmp_fifo_len;i++)
            {
                fifo_out(&uart->tx_fifo, &tmp_char, 1);
                putchar(tmp_char);
            }

            uart->tx_needs_flush = 0;
        }
    }

    /* check if rx irq */
    if(uart->irq_enabled_rx_data_available)
    {
        if(uart->fifo_enabled)
        {
            if(fifo_len(&uart->rx_fifo) >= uart->rx_irq_fifo_level)
                irq_trigger = 1;

            // printf("FIFO: %d %d\n", fifo_len(&uart->rx_fifo), uart->rx_irq_fifo_level);
        }
        else
        {
            if(CHECK_BIT(uart->regs[REG_LSR], 0))
                irq_trigger = 1;
        }
    }

    return irq_trigger;
}

void uart_add_rx_char(uart_16550a_td *uart, uint8_t x)
{
    pthread_mutex_lock(&uart->lock);

    if(uart->fifo_enabled)
    {
        fifo_in(&uart->rx_fifo, &x, 1);
    }
    else
    {
        uart->rx_val = x;
    }

    assign_u8_bit(&uart->regs[REG_LSR], 0, 1);

    pthread_mutex_unlock(&uart->lock);
}
