#ifndef RISCV_SOC_H
#define RISCV_SOC_H

#include <riscv_config.h>
#include <riscv_core.h>

typedef struct rv32_soc_struct
{
    rv32_core_td rv32_core;
    uint32_t ram[NR_RAM_WORDS];

} rv32_soc_td;

uint32_t rv32_soc_read_mem(void *priv, uint32_t address);
void rv32_soc_write_mem(void *priv, uint32_t address, uint32_t value, uint8_t nr_bytes);
void rv32_soc_dump_mem(rv32_soc_td *rv32_soc);
void rv32_soc_init(rv32_soc_td *rv32_soc, char *fw_file_name);

#endif /* RISCV_SOC_H */