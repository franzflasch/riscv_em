#ifndef RISCV_SOC_H
#define RISCV_SOC_H

#include <riscv_config.h>
#include <riscv_core.h>

typedef struct rv_soc_struct
{
    rv_core_td rv_core;
    uint32_t ram[NR_RAM_WORDS];

} rv_soc_td;

uint32_t rv_soc_read_mem(void *priv, uint32_t address);
void rv_soc_write_mem(void *priv, uint32_t address, uint32_t value, uint8_t nr_bytes);
void rv_soc_dump_mem(rv_soc_td *rv_soc);
void rv_soc_init(rv_soc_td *rv_soc, char *fw_file_name);

#endif /* RISCV_SOC_H */