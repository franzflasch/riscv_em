#ifndef RISCV_EXAMPLE_SOC_H
#define RISCV_EXAMPLE_SOC_H

#include <riscv_config.h>
#include <riscv_core.h>

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
    uint8_t __attribute__((aligned (4))) ram[RAM_SIZE_BYTES];

    rv_soc_mem_access_cb_td mem_access_cbs[3];

} rv_soc_td;

void rv_soc_dump_mem(rv_soc_td *rv_soc);
void rv_soc_init(rv_soc_td *rv_soc, char *fw_file_name);

#endif /* RISCV_EXAMPLE_SOC_H */
