#include <stdio.h>
#include <string.h>

#include <pthread.h>

#include <riscv_helper.h>
#include <uart_8250.h>

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

void uart_init(uart_ns8250_td *uart)
{
    memset(uart, 0, sizeof(uart_ns8250_td));

    if (pthread_mutex_init(&uart->lock, NULL) != 0)
    {
        die_msg("uart mutex init failed\n");
    }

    fifo_init(&uart->tx_fifo, uart->tx_fifo_data, UART_NS8250_FIFO_SIZE);
    fifo_init(&uart->rx_fifo, uart->rx_fifo_data, UART_NS8250_FIFO_SIZE);

    /* no interrupt pending */
    // uart->regs[REG_IIR] = NO_IRQ_PENDING;
    uart->rx_irq_fifo_level = 1;
    uart->curr_iir_id = 1;
}

int uart_write(void *priv, rv_uint_xlen address_internal, rv_uint_xlen val, uint8_t nr_bytes)
{
    uart_ns8250_td *uart = priv;
    uint8_t tmp_bits = 0;
    uint8_t val_u8 = val;

    pthread_mutex_lock(&uart->lock);

    if(nr_bytes != 1)
        die_msg("UART WRITE: Only single byte access allowed!\n");

    switch(address_internal)
    {
        case REG_RX_TX_DIV_LATCH_LO:
            if(!uart->dlab)
            {
                fifo_in(&uart->tx_fifo, &val_u8, 1);

                if( (!uart->fifo_enabled) || (val_u8 == '\n') )
                    uart->tx_needs_flush = 1;

                if(uart->regs[REG_IIR] == 2)
                {
                    uart->tx_stop_triggering = 0;
                }
            }
            else
            {
                /* we do not support any dlab registers */
                UART_DBG("DLAB Write Access " PRINTF_FMT " " PRINTF_FMT "!\n", address_internal, val_u8);
            }
        break;
        case REG_IER_LATCH_HI:
            if(!uart->dlab)
            {
                uart->irq_enabled_rx_data_available = extract8(val_u8, 0, 1);
                uart->irq_enabled_tx_holding_reg_empty = extract8(val_u8, 1, 1);
                uart->irq_enabled_rlsr_change = extract8(val_u8, 2, 1);
                uart->irq_enabled_msr_change = extract8(val_u8, 3, 1);
                uart->irq_enabled_sleep = extract8(val_u8, 4, 1);
                uart->irq_enabled_low_power = extract8(val_u8, 5, 1);

                uart->regs[REG_IER_LATCH_HI] = val_u8;

                // printf("\nIRQ ENABLE: rx_data: %x txh_empty: %x rlsr: %x msr: %x sleep: %x low_power: %x\n\n", 
                //     uart->irq_enabled_rx_data_available,
                //     uart->irq_enabled_tx_holding_reg_empty,
                //     uart->irq_enabled_rlsr_change,
                //     uart->irq_enabled_msr_change,
                //     uart->irq_enabled_sleep,
                //     uart->irq_enabled_low_power
                // );

                // if(uart->irq_enabled_rx_data_available)
                //     die_msg("UART RX Interrupts currently not supported!\n");

                // if(uart->irq_enabled_tx_holding_reg_empty)
                //     die_msg("UART TX Interrupts currently not supported!\n");

                // /* Currently we do not support any other interrupts */
                // tmp_val = extract32(val, 2, 6);
                // if(tmp_val)
                //     die_msg("Currently only RX and TX interrupts supported!\n");
            }
            else
            {
                /* we do not support any dlab registers */
                UART_DBG("DLAB Write Access " PRINTF_FMT " " PRINTF_FMT "!\n", address_internal, val_u8);
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
            // printf("Fifo enabled: %x\n", uart->fifo_enabled);

            tmp_bits = extract8(val, 6, 2);
            uart->rx_irq_fifo_level = (tmp_bits == 3) ? 14 : (tmp_bits == 2) ? 8 : (tmp_bits == 1) ? 4 : 1;
            // printf("fifo level %d\n", uart->rx_irq_fifo_level);
        break;
        case REG_LCR:
            uart->regs[REG_LCR] = val_u8;
            uart->dlab = extract8(val_u8, 7, 1);

            UART_DBG("LCR: "PRINTF_FMT"\n", val_u8);

            if(uart->dlab)
                UART_DBG("dlab activated\n");
            else
                UART_DBG("dlab deactivated\n");
        break;
        case REG_MCR:
            uart->regs[REG_MCR] = val_u8;
        break;
        default:
            die_msg("UART-Write Reg " PRINTF_FMT " not supported yet!\n", address_internal);
    }

    pthread_mutex_unlock(&uart->lock);

    return RV_MEM_ACCESS_OK;
}

int uart_read(void *priv, rv_uint_xlen address_internal, rv_uint_xlen *outval)
{
    uart_ns8250_td *uart = priv;
    uint8_t tmp_out_val = 0;

    pthread_mutex_lock(&uart->lock);

    switch(address_internal)
    {
        case REG_RX_TX_DIV_LATCH_LO:
            if(!uart->dlab)
            {
                fifo_out(&uart->rx_fifo, &tmp_out_val, 1);
                *outval = tmp_out_val;
                // printf("RX: %c %d\n", tmp_out_val, uart->lsr_change);
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
                tmp_out_val = ( (uart->irq_enabled_rx_data_available << 0) |
                                (uart->irq_enabled_tx_holding_reg_empty << 1) | 
                                (uart->irq_enabled_rlsr_change << 2) |
                                (uart->irq_enabled_msr_change << 3) |
                                (uart->irq_enabled_sleep << 4) |
                                (uart->irq_enabled_low_power << 5) );
                *outval = tmp_out_val;
            }
            else
            {
                /* we do not support any dlab registers */
                UART_DBG("DLAB Read Access " PRINTF_FMT "!\n", address_internal);
                *outval = 0;
            }
        break;
        case REG_IIR:
            /* 1 means no interrupt pending */
            // printf("RX: %x %d %d\n", uart->regs[REG_IIR], uart->lsr_change, fifo_len(&uart->rx_fifo));
            *outval = uart->regs[REG_IIR];
            // uart->wait_for_iir_read = 0;
            if(uart->regs[REG_IIR] == 2)
            {
                uart->curr_iir_id = NO_IRQ_PENDING;
                uart->tx_stop_triggering = 1;
            }
        break;
        case REG_LSR:
            {
                /* THR empty and line idle is always true here in our emulation */
                uint8_t data_avail = (!fifo_is_empty(&uart->rx_fifo)) & 1;
                uint8_t overrun_err = 0;
                uint8_t parity_err = 0;
                uint8_t framing_err = 0;
                uint8_t brk_sig = 0;
                uint8_t thr_empty = (fifo_is_empty(&uart->tx_fifo)) & 1;
                uint8_t thr_empty_and_idle = thr_empty;
                uint8_t err_data_fifo = 0;

                tmp_out_val = ( data_avail << 0 |
                                overrun_err << 1 |
                                parity_err << 2 |
                                framing_err << 3 |
                                brk_sig << 4 |
                                thr_empty << 5 |
                                thr_empty_and_idle << 6 |
                                err_data_fifo << 7
                            );

                // printf("LSR! %x\n", tmp_out_val);

                *outval = tmp_out_val;

                if(uart->lsr_change)
                    uart->lsr_change = 0;
            }
        break;
        case REG_LCR:
            *outval = uart->regs[REG_LCR];
        break;
        case REG_MSR:
            /* Not supported currently */
            *outval = 0xb0;
        break;
        case REG_MCR:
            /* Not supported currently */
            *outval = 0x8;
        break;
        default:
            die_msg("UART-Read Reg " PRINTF_FMT " not supported yet!\n", address_internal);
    }

    pthread_mutex_unlock(&uart->lock);

    return RV_MEM_ACCESS_OK;
}

uint8_t uart_update(void *priv)
{
    uart_ns8250_td *uart = priv;
    int i = 0;
    uint8_t tmp_char = 0;
    uint8_t tmp_fifo_len = 0;
    uint8_t irq_trigger = 0;

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
    }

    if( (uart->irq_enabled_rlsr_change || uart->irq_enabled_rx_data_available ) && uart->lsr_change )
    {
        // printf("RX\n");
        irq_trigger = 1;
        uart->curr_iir_id = 0xC;
        // uart->regs[REG_IIR] = 0xC;
    }
    else if( uart->irq_enabled_rx_data_available && (fifo_len(&uart->rx_fifo) >= uart->rx_irq_fifo_level) )
    {
        // printf("RX\n");
        irq_trigger = 1;
        uart->curr_iir_id = 0x4;
        // uart->regs[REG_IIR] = 0xC;
    }
    else if( (uart->irq_enabled_tx_holding_reg_empty) && fifo_is_empty(&uart->tx_fifo) && (!uart->tx_stop_triggering) )
    {
        // printf("TX\n");
        irq_trigger = 1;
        uart->curr_iir_id = 0x2;
    }

    uart->regs[REG_IIR] = uart->curr_iir_id;

    pthread_mutex_unlock(&uart->lock);

    return irq_trigger;
}

void uart_add_rx_char(uart_ns8250_td *uart, uint8_t x)
{
    pthread_mutex_lock(&uart->lock);

    // uint8_t tmp = 13;
    fifo_in(&uart->rx_fifo, &x, 1);
    // fifo_in(&uart->rx_fifo, &tmp, 1);
    uart->lsr_change = 1;

    // printf("RX %x\n", x);
    // assign_u8_bit(&uart->regs[REG_LSR], 0, 1);

    pthread_mutex_unlock(&uart->lock);
}
