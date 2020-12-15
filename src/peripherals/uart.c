#include <stdio.h>
#include <string.h>

#include <riscv_helper.h>

#include <uart.h>

/*** DLAB-0 REGS ***/
/* READ */
#define REG_RX 0
/* WRITE */
#define REG_TX 0
/* READ/WRITE */
#define REG_IER 1

/*** DLAB-1 REGS ***/
#define REG_DIV_LATCH_LO 0
#define REG_DIV_LATCH_HI 1

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

void uart_init(uart_16550a_td *uart)
{
    memset(uart, 0, sizeof(uart_16550a_td));
}

int uart_write(void *priv, rv_uint_xlen address_internal, rv_uint_xlen val, uint8_t nr_bytes)
{
    (void) priv;
    (void) nr_bytes;

    if(nr_bytes != 1)
        die_msg("UART WRITE: Only single byte access allowed!\n");

    switch(address_internal)
    {
        case REG_TX:
            putchar((char) val);
        break;
        default:
            die_msg("UART: Reg " PRINTF_FMT " not supported yet!\n", address_internal);
    }

    return RV_MEM_ACCESS_OK;
}

int uart_read(void *priv, rv_uint_xlen address_internal, rv_uint_xlen *outval)
{
    (void) priv;
    (void) address_internal;
    (void) outval;
    die_msg("UART READ currently not supported!\n");
    return RV_MEM_ACCESS_OK;
}
