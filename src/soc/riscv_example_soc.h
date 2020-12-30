#ifndef RISCV_EXAMPLE_SOC_H
#define RISCV_EXAMPLE_SOC_H

#include <riscv_config.h>
#include <core.h>

#include <plic.h>
#include <uart.h>

typedef struct rv_soc_mem_access_cb_struct
{
    int (*read)(void *priv, rv_uint_xlen address_internal, rv_uint_xlen *outval);
    int (*write)(void *priv, rv_uint_xlen address_internal, rv_uint_xlen val, uint8_t nr_bytes);
    void *priv;
    rv_uint_xlen addr_start;
    rv_uint_xlen mem_size;

} rv_soc_mem_access_cb_td;

typedef struct rv_soc_struct
{
    /* For now we have 1 single core */
    rv_core_td rv_core0;
    uint8_t *mrom; /* Contains reset vector and device-tree? */
    uint8_t *ram;

    clint_td clint;
    plic_td plic;
    uart_16550a_td uart;

    rv_soc_mem_access_cb_td mem_access_cbs[5];

} rv_soc_td;

void rv_soc_dump_mem(rv_soc_td *rv_soc);
void rv_soc_init(rv_soc_td *rv_soc, char *fw_file_name, char *dtb_file_name);
void rv_soc_run(rv_soc_td *rv_soc, rv_uint_xlen success_pc, uint64_t num_cycles);

#endif /* RISCV_EXAMPLE_SOC_H */